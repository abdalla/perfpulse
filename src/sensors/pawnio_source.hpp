#pragma once

#include <windows.h>

#include <memory>
#include <optional>

#include "core/win_handle.hpp"
#include "sensors/cpu_thermal_source.hpp"

namespace perfpulse::sensors {

// Reads CPU package temperature and RAPL energy through the PawnIO driver
// (https://pawnio.eu), the signed successor of WinRing0. PawnIO executes
// small signed modules; PerfPulse loads IntelMSR.bin or AMDFamily17.bin from
// the "modules" folder next to the executable.
class PawnIoSource final : public CpuThermalSource {
public:
    static std::unique_ptr<PawnIoSource> create();
    ~PawnIoSource() override;

    std::string sourceName() const override;
    float tjMax() const override { return tjMax_; }
    void read(session::SensorSample& sample) override;

private:
    enum class Vendor { Intel, Amd };

    PawnIoSource() = default;
    bool initialize();
    std::optional<uint64_t> readMsr(uint32_t index) const;
    std::optional<uint32_t> readSmn(uint32_t address) const;

    using ExecuteFn = HRESULT(WINAPI*)(HANDLE, const char*, const ULONG64*, SIZE_T, ULONG64*, SIZE_T, SIZE_T*);
    using CloseFn = HRESULT(WINAPI*)(HANDLE);

    core::UniqueModule library_;
    HANDLE executor_ = nullptr;
    ExecuteFn execute_ = nullptr;
    CloseFn close_ = nullptr;
    Vendor vendor_ = Vendor::Intel;
    float tjMax_ = session::kNoValue;
    double energyUnitJoules_ = 0.0;
    uint32_t lastEnergy_ = 0;
    int64_t lastEnergyQpc_ = 0;
};

}  // namespace perfpulse::sensors
