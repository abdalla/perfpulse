#pragma once

#include <vector>

#include "session/session_data.hpp"

namespace perfpulse::analysis {

struct ComponentEnergy {
    bool measured = false;
    double wattHours = 0.0;
    double averageWatts = 0.0;
    double peakWatts = 0.0;
};

struct EnergySummary {
    ComponentEnergy gpu;
    ComponentEnergy cpu;
    double totalWattHours = 0.0;      // GPU + CPU (measured only)
    double averageWatts = 0.0;
    double fpsPerWatt = 0.0;          // active average FPS / average measured watts
    double millijoulesPerFrame = 0.0;
};

// Integrates power over time (trapezoid rule over the sample timeline).
EnergySummary analyzeEnergy(const std::vector<session::SensorSample>& samples, double averageFps,
                            size_t frameCount);

}  // namespace perfpulse::analysis
