#pragma once

#include <memory>
#include <string>

#include "sensors/cpu_thermal_source.hpp"

struct IWbemServices;

namespace perfpulse::sensors {

// Reads CPU package temperature and power from LibreHardwareMonitor's WMI
// provider (namespace root\LibreHardwareMonitor) while LHM is running.
// The calling thread must have COM initialized.
class LhmSource final : public CpuThermalSource {
public:
    static std::unique_ptr<LhmSource> create();
    ~LhmSource() override;

    std::string sourceName() const override { return "LibreHardwareMonitor (WMI)"; }
    void read(session::SensorSample& sample) override;

private:
    LhmSource() = default;
    bool connect();
    bool locateSensors();

    IWbemServices* services_ = nullptr;
    std::wstring temperatureId_;
    std::wstring powerId_;
};

}  // namespace perfpulse::sensors
