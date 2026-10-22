#pragma once

#include <windows.h>

#include <memory>
#include <string>

#include "core/win_handle.hpp"
#include "sensors/gpu_backend.hpp"
#include "sensors/pdh_query.hpp"

namespace perfpulse::sensors {

// Vendor-neutral GPU telemetry for AMD, Intel or any WDDM 2.4+ adapter:
//  * usage and dedicated VRAM from the "GPU Engine"/"GPU Adapter Memory"
//    performance counters (what Task Manager shows);
//  * temperature, fan, clocks and voltage from D3DKMT adapter/node perf data,
//    when the driver implements them.
// Board power in watts is not part of WDDM, so it stays unavailable here.
class GenericGpu final : public GpuBackend {
public:
    static std::unique_ptr<GenericGpu> create();
    ~GenericGpu() override;

    std::string sourceName() const override { return "WDDM (D3DKMT + PDH)"; }
    std::string gpuName() const override { return name_; }
    std::string driverVersion() const override { return driver_; }
    void read(session::SensorSample& sample, const SensorRequest& request) override;

private:
    GenericGpu() = default;
    bool initialize();

    LUID luid_{};
    std::wstring luidTag_;  // "luid_0x00000000_0x0000D1E2" as used by PDH instances
    unsigned adapterHandle_ = 0;
    float vramTotalGb_ = session::kNoValue;
    std::string name_;
    std::string driver_;
    core::UniqueModule gdi_;
    void* queryAdapterInfo_ = nullptr;
    void* closeAdapter_ = nullptr;
    PdhQuery pdh_;
    int engineCounter_ = -1;
    int memoryCounter_ = -1;
};

}  // namespace perfpulse::sensors
