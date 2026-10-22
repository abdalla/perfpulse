#pragma once

#include <memory>

#include "core/win_handle.hpp"
#include "sensors/cpu_thermal_source.hpp"

namespace perfpulse::sensors {

// Reads "CPU Package" temperature and power from HWiNFO's shared memory
// (Settings -> "Shared Memory Support" must be enabled in HWiNFO).
class HwinfoSource final : public CpuThermalSource {
public:
    static std::unique_ptr<HwinfoSource> create();
    ~HwinfoSource() override;

    std::string sourceName() const override { return "HWiNFO (shared memory)"; }
    void read(session::SensorSample& sample) override;

private:
    HwinfoSource() = default;
    bool locateReadings();

    core::UniqueHandle mapping_;
    const unsigned char* view_ = nullptr;
    long temperatureIndex_ = -1;
    long powerIndex_ = -1;
};

}  // namespace perfpulse::sensors
