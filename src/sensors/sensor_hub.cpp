#include "sensors/sensor_hub.hpp"

#include <objbase.h>

#include "core/i18n.hpp"
#include "sensors/generic_gpu.hpp"
#include "sensors/hwinfo_source.hpp"
#include "sensors/lhm_source.hpp"
#include "sensors/memory_sensors.hpp"
#include "sensors/nvidia_gpu.hpp"
#include "sensors/pawnio_source.hpp"
#include "sensors/system_info.hpp"

namespace perfpulse::sensors {

std::unique_ptr<GpuBackend> createGpuBackend() {
    if (auto nvidia = NvidiaGpu::create()) {
        return nvidia;
    }
    return GenericGpu::create();
}

std::unique_ptr<CpuThermalSource> createCpuThermalSource() {
    if (auto pawnio = PawnIoSource::create()) {
        return pawnio;
    }
    if (auto hwinfo = HwinfoSource::create()) {
        return hwinfo;
    }
    return LhmSource::create();
}

SensorHub::SensorHub() {
    comInitialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    gpu_ = createGpuBackend();
    cpuThermal_ = createCpuThermalSource();
}

SensorHub::~SensorHub() {
    gpu_.reset();
    cpuThermal_.reset();
    if (comInitialized_) {
        CoUninitialize();
    }
}

void SensorHub::read(session::SensorSample& sample, const SensorRequest& request) {
    if (gpu_) {
        gpu_->read(sample, request);
    }
    cpu_.read(sample, request);
    if (cpuThermal_) {
        cpuThermal_->read(sample);
    }
    readSystemMemory(sample);
}

void SensorHub::describe(session::HardwareInfo& hardware, std::string& gpuSource, std::string& cpuSource) const {
    hardware = querySystemInfo();
    if (gpu_) {
        hardware.gpuName = gpu_->gpuName();
        hardware.gpuDriver = gpu_->driverVersion();
        hardware.gpuPowerLimitW = gpu_->powerLimitWatts();
        gpuSource = gpu_->sourceName();
    } else {
        gpuSource = std::string(core::tr("source.none"));
    }
    if (cpuThermal_) {
        cpuSource = cpuThermal_->sourceName();
        hardware.cpuTjMax = cpuThermal_->tjMax();
    } else {
        cpuSource = std::string(core::tr("source.none"));
    }
    // Sensible defaults when TjMax could not be read: 100 °C for recent Intel desktop parts, 95 °C for Zen 4.
    if (!session::hasValue(hardware.cpuTjMax)) {
        hardware.cpuTjMax = hardware.cpuName.find("AMD") != std::string::npos ? 95.0f : 100.0f;
    }
}

}  // namespace perfpulse::sensors
