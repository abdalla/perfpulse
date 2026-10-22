#include "sensors/nvidia_gpu.hpp"

#include <cstring>
#include <type_traits>

#include "core/log.hpp"

namespace perfpulse::sensors {

namespace {

// --- NVML (subset of nvml.h) -------------------------------------------------
using NvmlReturn = int;
using NvmlDevice = void*;
constexpr NvmlReturn kNvmlSuccess = 0;
constexpr int kNvmlTemperatureGpu = 0;
constexpr int kNvmlClockGraphics = 0;
constexpr int kNvmlClockMemory = 2;

struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct NvmlPciInfo {
    char busIdLegacy[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
    char busId[32];
};

// Clock event (throttle) reasons.
constexpr unsigned long long kReasonGpuIdle = 0x1;
constexpr unsigned long long kReasonSwPowerCap = 0x4;
constexpr unsigned long long kReasonHwSlowdown = 0x8;
constexpr unsigned long long kReasonSyncBoost = 0x10;
constexpr unsigned long long kReasonSwThermal = 0x20;
constexpr unsigned long long kReasonHwThermal = 0x40;
constexpr unsigned long long kReasonHwPowerBrake = 0x80;

// --- NVAPI (queried by interface id) ------------------------------------------
using NvApiStatus = int;
using NvPhysicalGpu = void*;
constexpr NvApiStatus kNvApiOk = 0;
constexpr unsigned kNvApiInitialize = 0x0150E828;
constexpr unsigned kNvApiUnload = 0xD22BDD7E;
constexpr unsigned kNvApiEnumPhysicalGpus = 0xE5AC921F;
constexpr unsigned kNvApiGetBusId = 0x1BE0B8E5;
constexpr unsigned kNvApiThermalGetSensors = 0x65FE3AAD;
constexpr unsigned kNvApiVoltRailsGetStatus = 0x465F9BCF;

// Undocumented thermal sensor table, layout as used by LibreHardwareMonitor.
struct NvThermalSensors {
    unsigned version;
    unsigned mask;
    int reserved[8];
    int temperatures[32];  // Q8.8 fixed point °C
};
constexpr int kThermalHotspotIndex = 1;
constexpr int kThermalMemoryJunctionIndex = 9;

struct NvVoltageStatus {
    unsigned version;
    unsigned flags;
    unsigned reserved1[8];
    unsigned valueMicrovolts;
    unsigned reserved2[8];
};

template <typename Fn>
bool load(const core::UniqueModule& module, Fn& fn, const char* name) {
    fn = module.proc<Fn>(name);
    return fn != nullptr;
}

}  // namespace

struct NvidiaGpu::Nvml {
    NvmlReturn (*init)() = nullptr;
    NvmlReturn (*shutdown)() = nullptr;
    NvmlReturn (*getCount)(unsigned*) = nullptr;
    NvmlReturn (*getHandleByIndex)(unsigned, NvmlDevice*) = nullptr;
    NvmlReturn (*getName)(NvmlDevice, char*, unsigned) = nullptr;
    NvmlReturn (*getDriverVersion)(char*, unsigned) = nullptr;
    NvmlReturn (*getUtilization)(NvmlDevice, NvmlUtilization*) = nullptr;
    NvmlReturn (*getTemperature)(NvmlDevice, int, unsigned*) = nullptr;
    NvmlReturn (*getClock)(NvmlDevice, int, unsigned*) = nullptr;
    NvmlReturn (*getPower)(NvmlDevice, unsigned*) = nullptr;
    NvmlReturn (*getPowerLimit)(NvmlDevice, unsigned*) = nullptr;
    NvmlReturn (*getFanSpeed)(NvmlDevice, unsigned*) = nullptr;
    NvmlReturn (*getMemory)(NvmlDevice, NvmlMemory*) = nullptr;
    NvmlReturn (*getReasons)(NvmlDevice, unsigned long long*) = nullptr;
    NvmlReturn (*getPciInfo)(NvmlDevice, NvmlPciInfo*) = nullptr;
};

struct NvidiaGpu::NvApi {
    void* (*queryInterface)(unsigned) = nullptr;
    NvApiStatus (*initialize)() = nullptr;
    NvApiStatus (*unload)() = nullptr;
    NvApiStatus (*enumPhysicalGpus)(NvPhysicalGpu*, unsigned*) = nullptr;
    NvApiStatus (*getBusId)(NvPhysicalGpu, unsigned*) = nullptr;
    NvApiStatus (*thermalGetSensors)(NvPhysicalGpu, NvThermalSensors*) = nullptr;
    NvApiStatus (*voltRailsGetStatus)(NvPhysicalGpu, NvVoltageStatus*) = nullptr;
};

std::unique_ptr<NvidiaGpu> NvidiaGpu::create() {
    std::unique_ptr<NvidiaGpu> gpu(new NvidiaGpu());
    if (!gpu->initializeNvml()) {
        return nullptr;
    }
    gpu->initializeNvApi();
    return gpu;
}

NvidiaGpu::~NvidiaGpu() {
    if (nvapi_ && nvapi_->unload) {
        nvapi_->unload();
    }
    if (nvml_ && nvml_->shutdown) {
        nvml_->shutdown();
    }
}

bool NvidiaGpu::initializeNvml() {
    nvmlModule_.reset(LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!nvmlModule_) {
        nvmlModule_.reset(LoadLibraryExW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll", nullptr,
                                         LOAD_WITH_ALTERED_SEARCH_PATH));
    }
    if (!nvmlModule_) {
        return false;
    }
    nvml_ = std::make_unique<Nvml>();
    auto& n = *nvml_;
    const auto& m = nvmlModule_;
    if (!load(m, n.init, "nvmlInit_v2") || !load(m, n.getCount, "nvmlDeviceGetCount_v2") ||
        !load(m, n.getHandleByIndex, "nvmlDeviceGetHandleByIndex_v2")) {
        return false;
    }
    load(m, n.shutdown, "nvmlShutdown");
    load(m, n.getName, "nvmlDeviceGetName");
    load(m, n.getDriverVersion, "nvmlSystemGetDriverVersion");
    load(m, n.getUtilization, "nvmlDeviceGetUtilizationRates");
    load(m, n.getTemperature, "nvmlDeviceGetTemperature");
    load(m, n.getClock, "nvmlDeviceGetClockInfo");
    load(m, n.getPower, "nvmlDeviceGetPowerUsage");
    load(m, n.getPowerLimit, "nvmlDeviceGetEnforcedPowerLimit");
    load(m, n.getFanSpeed, "nvmlDeviceGetFanSpeed");
    load(m, n.getMemory, "nvmlDeviceGetMemoryInfo");
    load(m, n.getPciInfo, "nvmlDeviceGetPciInfo_v3");
    if (!load(m, n.getReasons, "nvmlDeviceGetCurrentClocksEventReasons")) {
        load(m, n.getReasons, "nvmlDeviceGetCurrentClocksThrottleReasons");
    }

    if (n.init() != kNvmlSuccess) {
        n.shutdown = nullptr;
        return false;
    }
    unsigned count = 0;
    if (n.getCount(&count) != kNvmlSuccess || count == 0) {
        return false;
    }
    // With several NVIDIA GPUs, the one with the most memory is the gaming card.
    unsigned long long bestMemory = 0;
    for (unsigned i = 0; i < count; ++i) {
        NvmlDevice device = nullptr;
        if (n.getHandleByIndex(i, &device) != kNvmlSuccess) {
            continue;
        }
        NvmlMemory memory{};
        const bool hasMemory = n.getMemory && n.getMemory(device, &memory) == kNvmlSuccess;
        if (device_ == nullptr || (hasMemory && memory.total > bestMemory)) {
            device_ = device;
            bestMemory = memory.total;
        }
    }
    if (device_ == nullptr) {
        return false;
    }

    char buffer[96] = {};
    if (n.getName && n.getName(device_, buffer, sizeof(buffer)) == kNvmlSuccess) {
        name_ = buffer;
    }
    std::memset(buffer, 0, sizeof(buffer));
    if (n.getDriverVersion && n.getDriverVersion(buffer, sizeof(buffer)) == kNvmlSuccess) {
        driver_ = buffer;
    }
    unsigned limitMilliwatts = 0;
    if (n.getPowerLimit && n.getPowerLimit(device_, &limitMilliwatts) == kNvmlSuccess) {
        powerLimitW_ = static_cast<float>(limitMilliwatts) / 1000.0f;
    }
    core::log::info("NVML ready: " + name_ + " driver " + driver_);
    return true;
}

void NvidiaGpu::initializeNvApi() {
    nvapiModule_.reset(LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!nvapiModule_) {
        return;
    }
    auto api = std::make_unique<NvApi>();
    if (!load(nvapiModule_, api->queryInterface, "nvapi_QueryInterface")) {
        return;
    }
    const auto query = [&api](auto& fn, unsigned id) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(api->queryInterface(id));
        return fn != nullptr;
    };
    if (!query(api->initialize, kNvApiInitialize) || api->initialize() != kNvApiOk) {
        return;
    }
    query(api->unload, kNvApiUnload);
    query(api->enumPhysicalGpus, kNvApiEnumPhysicalGpus);
    query(api->getBusId, kNvApiGetBusId);
    query(api->thermalGetSensors, kNvApiThermalGetSensors);
    query(api->voltRailsGetStatus, kNvApiVoltRailsGetStatus);
    if (!api->enumPhysicalGpus) {
        nvapi_ = std::move(api);
        return;
    }

    NvPhysicalGpu handles[64] = {};
    unsigned count = 0;
    if (api->enumPhysicalGpus(handles, &count) != kNvApiOk || count == 0) {
        nvapi_ = std::move(api);
        return;
    }
    // Match the NVML device by PCI bus so both APIs describe the same card.
    NvmlPciInfo pci{};
    const bool hasPci = nvml_->getPciInfo && nvml_->getPciInfo(device_, &pci) == kNvmlSuccess;
    nvapiGpu_ = handles[0];
    for (unsigned i = 0; hasPci && api->getBusId && i < count; ++i) {
        unsigned bus = 0;
        if (api->getBusId(handles[i], &bus) == kNvApiOk && bus == pci.bus) {
            nvapiGpu_ = handles[i];
            break;
        }
    }

    // Probe how many thermal sensors the board exposes.
    if (api->thermalGetSensors) {
        unsigned maxBit = 0;
        for (; maxBit < 32; ++maxBit) {
            NvThermalSensors sensors{};
            sensors.version = sizeof(NvThermalSensors) | (2u << 16);
            sensors.mask = 1u << maxBit;
            if (api->thermalGetSensors(nvapiGpu_, &sensors) != kNvApiOk) {
                break;
            }
        }
        thermalMask_ = maxBit == 0 ? 0 : (maxBit >= 32 ? 0xFFFFFFFFu : (1u << maxBit) - 1);
    }
    nvapi_ = std::move(api);
}

std::string NvidiaGpu::sourceName() const {
    return nvapi_ && nvapiGpu_ ? "NVML + NVAPI" : "NVML";
}

void NvidiaGpu::read(session::SensorSample& s, const SensorRequest& request) {
    using core::Metric;
    const auto& n = *nvml_;

    NvmlUtilization utilization{};
    if (n.getUtilization && n.getUtilization(device_, &utilization) == kNvmlSuccess) {
        s.gpuUsage = static_cast<float>(utilization.gpu);
    }
    unsigned value = 0;
    if (request.wants(Metric::GpuTemperature) && n.getTemperature &&
        n.getTemperature(device_, kNvmlTemperatureGpu, &value) == kNvmlSuccess) {
        s.gpuTemperature = static_cast<float>(value);
    }
    if (request.wants(Metric::GpuCoreClock) && n.getClock &&
        n.getClock(device_, kNvmlClockGraphics, &value) == kNvmlSuccess) {
        s.gpuCoreClock = static_cast<float>(value);
    }
    if (request.wants(Metric::GpuMemoryClock) && n.getClock &&
        n.getClock(device_, kNvmlClockMemory, &value) == kNvmlSuccess) {
        s.gpuMemoryClock = static_cast<float>(value);
    }
    if (n.getPower && n.getPower(device_, &value) == kNvmlSuccess) {
        s.gpuPower = static_cast<float>(value) / 1000.0f;
    }
    if (request.wants(Metric::GpuFan) && n.getFanSpeed && n.getFanSpeed(device_, &value) == kNvmlSuccess) {
        s.gpuFan = static_cast<float>(value);
    }
    NvmlMemory memory{};
    if (n.getMemory && n.getMemory(device_, &memory) == kNvmlSuccess) {
        constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
        s.vramUsedGb = static_cast<float>(static_cast<double>(memory.used) / kGiB);
        s.vramTotalGb = static_cast<float>(static_cast<double>(memory.total) / kGiB);
    }
    unsigned long long reasons = 0;
    if (n.getReasons && n.getReasons(device_, &reasons) == kNvmlSuccess) {
        uint32_t limiters = session::kLimiterNone;
        if (reasons & (kReasonSwPowerCap | kReasonHwPowerBrake)) {
            limiters |= session::kLimiterPower;
        }
        if (reasons & (kReasonSwThermal | kReasonHwThermal)) {
            limiters |= session::kLimiterThermal;
        }
        if (reasons & kReasonHwSlowdown) {
            limiters |= session::kLimiterHardwareSlowdown;
        }
        if (reasons & kReasonGpuIdle) {
            limiters |= session::kLimiterIdle;
        }
        if (reasons & kReasonSyncBoost) {
            limiters |= session::kLimiterSync;
        }
        s.gpuLimiters = limiters;
    }
    readNvApi(s, request);
}

void NvidiaGpu::readNvApi(session::SensorSample& s, const SensorRequest& request) {
    using core::Metric;
    if (!nvapi_ || nvapiGpu_ == nullptr) {
        return;
    }
    const bool wantsThermal = request.wants(Metric::GpuHotspot) || request.wants(Metric::VramTemperature);
    if (wantsThermal && nvapi_->thermalGetSensors && thermalMask_ != 0) {
        NvThermalSensors sensors{};
        sensors.version = sizeof(NvThermalSensors) | (2u << 16);
        sensors.mask = thermalMask_;
        if (nvapi_->thermalGetSensors(nvapiGpu_, &sensors) == kNvApiOk) {
            const int hotspot = sensors.temperatures[kThermalHotspotIndex];
            const int junction = sensors.temperatures[kThermalMemoryJunctionIndex];
            if (hotspot > 0) {
                s.gpuHotspot = static_cast<float>(hotspot) / 256.0f;
            }
            if (junction > 0) {
                s.vramTemperature = static_cast<float>(junction) / 256.0f;
            }
        }
    }
    if (request.wants(Metric::GpuVoltage) && nvapi_->voltRailsGetStatus && voltageSupported_) {
        NvVoltageStatus status{};
        status.version = sizeof(NvVoltageStatus) | (1u << 16);
        if (nvapi_->voltRailsGetStatus(nvapiGpu_, &status) == kNvApiOk && status.valueMicrovolts > 0) {
            s.gpuVoltage = static_cast<float>(status.valueMicrovolts) / 1'000'000.0f;
        } else {
            voltageSupported_ = false;  // Do not keep paying for a call that never works.
        }
    }
}

}  // namespace perfpulse::sensors
