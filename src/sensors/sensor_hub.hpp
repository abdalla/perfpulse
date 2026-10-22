#pragma once

#include <memory>
#include <string>

#include "sensors/cpu_sensors.hpp"
#include "sensors/cpu_thermal_source.hpp"
#include "sensors/gpu_backend.hpp"
#include "sensors/sensor_request.hpp"
#include "session/session_data.hpp"

namespace perfpulse::sensors {

// Owns every sensor backend and fills one SensorSample per call. Created on
// the sampler thread (COM/WMI and PDH handles stay on that thread) and only
// alive while a session is being recorded.
class SensorHub {
public:
    SensorHub();
    ~SensorHub();
    SensorHub(const SensorHub&) = delete;
    SensorHub& operator=(const SensorHub&) = delete;

    void read(session::SensorSample& sample, const SensorRequest& request);

    // Hardware description plus which backends are in use.
    void describe(session::HardwareInfo& hardware, std::string& gpuSource, std::string& cpuSource) const;

private:
    std::unique_ptr<GpuBackend> gpu_;
    std::unique_ptr<CpuThermalSource> cpuThermal_;
    CpuSensors cpu_;
    bool comInitialized_ = false;
};

}  // namespace perfpulse::sensors
