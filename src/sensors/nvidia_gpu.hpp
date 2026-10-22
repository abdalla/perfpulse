#pragma once

#include <memory>

#include "core/win_handle.hpp"
#include "sensors/gpu_backend.hpp"

namespace perfpulse::sensors {

// NVIDIA GPUs through NVML (documented, ships with the driver) plus a few
// NVAPI queries for what NVML does not expose on GeForce cards: hotspot,
// memory junction temperature and core voltage. Both DLLs are loaded at
// runtime; nothing NVIDIA-specific is linked into the executable.
class NvidiaGpu final : public GpuBackend {
public:
    static std::unique_ptr<NvidiaGpu> create();
    ~NvidiaGpu() override;

    std::string sourceName() const override;
    std::string gpuName() const override { return name_; }
    std::string driverVersion() const override { return driver_; }
    float powerLimitWatts() const override { return powerLimitW_; }
    void read(session::SensorSample& sample, const SensorRequest& request) override;

private:
    struct Nvml;
    struct NvApi;

    NvidiaGpu() = default;
    bool initializeNvml();
    void initializeNvApi();
    void readNvApi(session::SensorSample& sample, const SensorRequest& request);

    core::UniqueModule nvmlModule_;
    core::UniqueModule nvapiModule_;
    std::unique_ptr<Nvml> nvml_;
    std::unique_ptr<NvApi> nvapi_;
    void* device_ = nullptr;     // nvmlDevice_t
    void* nvapiGpu_ = nullptr;   // NvPhysicalGpuHandle
    unsigned thermalMask_ = 0;
    bool voltageSupported_ = true;
    std::string name_;
    std::string driver_;
    float powerLimitW_ = session::kNoValue;
};

}  // namespace perfpulse::sensors
