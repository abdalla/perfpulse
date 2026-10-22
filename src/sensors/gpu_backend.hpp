#pragma once

#include <memory>
#include <string>

#include "sensors/sensor_request.hpp"
#include "session/session_data.hpp"

namespace perfpulse::sensors {

class GpuBackend {
public:
    virtual ~GpuBackend() = default;

    virtual std::string sourceName() const = 0;  // e.g. "NVML + NVAPI"
    virtual std::string gpuName() const = 0;
    virtual std::string driverVersion() const = 0;
    virtual float powerLimitWatts() const { return session::kNoValue; }

    // Fills the GPU and VRAM fields of the sample.
    virtual void read(session::SensorSample& sample, const SensorRequest& request) = 0;
};

// Picks the richest backend available: vendor API first, then the
// vendor-neutral WDDM/PDH counters.
std::unique_ptr<GpuBackend> createGpuBackend();

}  // namespace perfpulse::sensors
