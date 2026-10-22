#pragma once

#include <windows.h>

#include "core/metrics.hpp"

namespace perfpulse::sensors {

// What the sampler needs on this tick. Sensors not requested are skipped so a
// minimal selection costs almost nothing.
struct SensorRequest {
    core::MetricSet metrics;
    DWORD gameProcessId = 0;

    bool wants(core::Metric metric) const { return metrics.has(metric); }
};

// Readings that feed the bottleneck and energy analysis are always collected,
// whatever the user chose to display.
inline SensorRequest makeSensorRequest(const core::MetricSet& displayed, DWORD gameProcessId) {
    using core::Metric;
    SensorRequest request{displayed, gameProcessId};
    for (const auto metric : {Metric::GpuUsage, Metric::GpuPower, Metric::GpuLimiter, Metric::VramUsedGb,
                              Metric::CpuUsage, Metric::CpuBusiestCore, Metric::CpuTemperature, Metric::CpuPower,
                              Metric::RamUsedGb}) {
        request.metrics.set(metric);
    }
    return request;
}

}  // namespace perfpulse::sensors
