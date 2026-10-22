#pragma once

#include <memory>
#include <string>

#include "session/session_data.hpp"

namespace perfpulse::sensors {

// Package temperature and power. These need ring-0 access (MSRs/SMN), so
// they come from a driver-backed source; see createCpuThermalSource().
class CpuThermalSource {
public:
    virtual ~CpuThermalSource() = default;
    virtual std::string sourceName() const = 0;
    virtual float tjMax() const { return session::kNoValue; }
    // Sets cpuTemperature / cpuPower when available.
    virtual void read(session::SensorSample& sample) = 0;
};

// Tries PawnIO, then HWiNFO shared memory, then LibreHardwareMonitor WMI.
// Returns nullptr when none is available; the report then shows N/A.
std::unique_ptr<CpuThermalSource> createCpuThermalSource();

}  // namespace perfpulse::sensors
