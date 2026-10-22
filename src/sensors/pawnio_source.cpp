#include "sensors/pawnio_source.hpp"

#include <intrin.h>

#include <cmath>
#include <cstring>
#include <filesystem>

#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"

namespace perfpulse::sensors {

namespace {

// Intel
constexpr uint32_t kMsrTemperatureTarget = 0x1A2;
constexpr uint32_t kMsrPackageThermStatus = 0x1B1;
constexpr uint32_t kMsrRaplPowerUnit = 0x606;
constexpr uint32_t kMsrPackageEnergyStatus = 0x611;
// AMD Zen (family 17h and later)
constexpr uint32_t kMsrAmdRaplPowerUnit = 0xC0010299;
constexpr uint32_t kMsrAmdPackageEnergy = 0xC001029B;
constexpr uint32_t kSmnThmTconCurTmp = 0x00059800;

std::string cpuVendor() {
    int registers[4] = {};
    __cpuid(registers, 0);
    char vendor[13] = {};
    std::memcpy(vendor, &registers[1], 4);
    std::memcpy(vendor + 4, &registers[3], 4);
    std::memcpy(vendor + 8, &registers[2], 4);
    return vendor;
}

int cpuFamily() {
    int registers[4] = {};
    __cpuid(registers, 1);
    const int family = (registers[0] >> 8) & 0xF;
    const int extended = (registers[0] >> 20) & 0xFF;
    return family == 0xF ? family + extended : family;
}

int64_t qpcNow() {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

double qpcFrequency() {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<double>(value.QuadPart);
}

std::optional<std::string> readModule(const wchar_t* fileName) {
    for (const auto& directory : {core::executableDirectory() / L"modules", core::executableDirectory()}) {
        if (auto content = core::readTextFile(directory / fileName)) {
            return content;
        }
    }
    return std::nullopt;
}

}  // namespace

std::unique_ptr<PawnIoSource> PawnIoSource::create() {
    std::unique_ptr<PawnIoSource> source(new PawnIoSource());
    return source->initialize() ? std::move(source) : nullptr;
}

PawnIoSource::~PawnIoSource() {
    if (executor_ != nullptr && close_ != nullptr) {
        close_(executor_);
    }
}

bool PawnIoSource::initialize() {
    const std::string vendor = cpuVendor();
    const wchar_t* moduleName = nullptr;
    if (vendor == "GenuineIntel") {
        vendor_ = Vendor::Intel;
        moduleName = L"IntelMSR.bin";
    } else if (vendor == "AuthenticAMD" && cpuFamily() >= 0x17) {
        vendor_ = Vendor::Amd;
        moduleName = L"AMDFamily17.bin";
    } else {
        return false;
    }

    library_.reset(LoadLibraryExW(L"C:\\Program Files\\PawnIO\\PawnIOLib.dll", nullptr,
                                  LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!library_) {
        return false;
    }
    const auto open = library_.proc<HRESULT(WINAPI*)(HANDLE*)>("pawnio_open");
    const auto load = library_.proc<HRESULT(WINAPI*)(HANDLE, const UCHAR*, SIZE_T)>("pawnio_load");
    execute_ = library_.proc<ExecuteFn>("pawnio_execute");
    close_ = library_.proc<CloseFn>("pawnio_close");
    if (open == nullptr || load == nullptr || execute_ == nullptr || close_ == nullptr) {
        return false;
    }
    const auto blob = readModule(moduleName);
    if (!blob) {
        core::log::warn("PawnIO is installed but modules\\" + core::toUtf8(moduleName) + " is missing");
        return false;
    }
    if (FAILED(open(&executor_))) {
        core::log::warn("pawnio_open failed (driver not running or not elevated)");
        executor_ = nullptr;
        return false;
    }
    if (FAILED(load(executor_, reinterpret_cast<const UCHAR*>(blob->data()), blob->size()))) {
        core::log::warn("pawnio_load rejected the module");
        return false;
    }

    if (vendor_ == Vendor::Intel) {
        if (const auto target = readMsr(kMsrTemperatureTarget)) {
            tjMax_ = static_cast<float>((*target >> 16) & 0xFF);
        }
        if (const auto unit = readMsr(kMsrRaplPowerUnit)) {
            energyUnitJoules_ = 1.0 / std::pow(2.0, static_cast<double>((*unit >> 8) & 0x1F));
        }
    } else {
        tjMax_ = 95.0f;  // Zen 4/5 default throttle point; Zen 2/3 desktop parts use 90-95 °C.
        if (const auto unit = readMsr(kMsrAmdRaplPowerUnit)) {
            energyUnitJoules_ = 1.0 / std::pow(2.0, static_cast<double>((*unit >> 8) & 0x1F));
        }
    }
    session::SensorSample primer;
    read(primer);
    core::log::info("PawnIO CPU sensors ready");
    return session::hasValue(primer.cpuTemperature) || energyUnitJoules_ > 0.0;
}

std::string PawnIoSource::sourceName() const {
    return vendor_ == Vendor::Intel ? "PawnIO (IntelMSR)" : "PawnIO (AMDFamily17)";
}

std::optional<uint64_t> PawnIoSource::readMsr(uint32_t index) const {
    const ULONG64 input = index;
    ULONG64 output = 0;
    SIZE_T returned = 0;
    if (FAILED(execute_(executor_, "ioctl_read_msr", &input, 1, &output, 1, &returned)) || returned < 1) {
        return std::nullopt;
    }
    return output;
}

std::optional<uint32_t> PawnIoSource::readSmn(uint32_t address) const {
    const ULONG64 input = address;
    ULONG64 output = 0;
    SIZE_T returned = 0;
    if (FAILED(execute_(executor_, "ioctl_read_smn", &input, 1, &output, 1, &returned)) || returned < 1) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(output);
}

void PawnIoSource::read(session::SensorSample& sample) {
    if (vendor_ == Vendor::Intel) {
        const auto status = readMsr(kMsrPackageThermStatus);
        if (status && session::hasValue(tjMax_) && (*status & (1ull << 31))) {  // reading valid
            sample.cpuTemperature = tjMax_ - static_cast<float>((*status >> 16) & 0x7F);
        }
    } else if (const auto raw = readSmn(kSmnThmTconCurTmp)) {
        float temperature = static_cast<float>((*raw >> 21) & 0x7FF) * 0.125f;
        if (*raw & (1u << 19)) {
            temperature -= 49.0f;  // "range select": reading offset by 49 °C
        }
        sample.cpuTemperature = temperature;
    }

    if (energyUnitJoules_ <= 0.0) {
        return;
    }
    const auto energy = readMsr(vendor_ == Vendor::Intel ? kMsrPackageEnergyStatus : kMsrAmdPackageEnergy);
    if (!energy) {
        return;
    }
    const auto counter = static_cast<uint32_t>(*energy);
    const int64_t now = qpcNow();
    if (lastEnergyQpc_ != 0) {
        const uint32_t delta = counter - lastEnergy_;  // unsigned arithmetic handles wraparound
        const double seconds = static_cast<double>(now - lastEnergyQpc_) / qpcFrequency();
        if (seconds > 0.05) {
            sample.cpuPower = static_cast<float>(static_cast<double>(delta) * energyUnitJoules_ / seconds);
        }
    }
    lastEnergy_ = counter;
    lastEnergyQpc_ = now;
}

}  // namespace perfpulse::sensors
