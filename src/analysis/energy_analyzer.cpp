#include "analysis/energy_analyzer.hpp"

#include <algorithm>

namespace perfpulse::analysis {

namespace {

using session::hasValue;
using session::SensorSample;

template <typename Getter>
ComponentEnergy integrate(const std::vector<SensorSample>& samples, Getter getter) {
    ComponentEnergy energy;
    double joules = 0.0;
    double coveredSec = 0.0;
    for (size_t i = 1; i < samples.size(); ++i) {
        const float previous = getter(samples[i - 1]);
        const float current = getter(samples[i]);
        if (!hasValue(previous) || !hasValue(current)) {
            continue;
        }
        const double dt = samples[i].timeSec - samples[i - 1].timeSec;
        if (dt <= 0.0 || dt > 30.0) {
            continue;
        }
        joules += 0.5 * (previous + current) * dt;
        coveredSec += dt;
    }
    for (const auto& sample : samples) {
        const float watts = getter(sample);
        if (hasValue(watts)) {
            energy.measured = true;
            energy.peakWatts = std::max(energy.peakWatts, static_cast<double>(watts));
        }
    }
    energy.wattHours = joules / 3600.0;
    energy.averageWatts = coveredSec > 0.0 ? joules / coveredSec : 0.0;
    return energy;
}

}  // namespace

EnergySummary analyzeEnergy(const std::vector<SensorSample>& samples, double averageFps, size_t frameCount) {
    EnergySummary summary;
    summary.gpu = integrate(samples, [](const SensorSample& s) { return s.gpuPower; });
    summary.cpu = integrate(samples, [](const SensorSample& s) { return s.cpuPower; });
    summary.totalWattHours = summary.gpu.wattHours + summary.cpu.wattHours;
    summary.averageWatts = summary.gpu.averageWatts + summary.cpu.averageWatts;
    if (summary.averageWatts > 0.0) {
        summary.fpsPerWatt = averageFps / summary.averageWatts;
    }
    if (frameCount > 0) {
        summary.millijoulesPerFrame = summary.totalWattHours * 3600.0 * 1000.0 / static_cast<double>(frameCount);
    }
    return summary;
}

}  // namespace perfpulse::analysis
