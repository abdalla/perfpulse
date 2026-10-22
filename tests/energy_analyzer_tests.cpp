#include "analysis/energy_analyzer.hpp"
#include "test_framework.hpp"

using namespace perfpulse;
using namespace perfpulse::analysis;

TEST_CASE("energy: integrates constant power") {
    std::vector<session::SensorSample> samples;
    for (int i = 0; i <= 3600; ++i) {
        session::SensorSample s;
        s.timeSec = i;
        s.gpuPower = 300.0f;
        s.cpuPower = 100.0f;
        samples.push_back(s);
    }
    const auto energy = analyzeEnergy(samples, 120.0, 432000);
    CHECK_NEAR(energy.gpu.wattHours, 300.0, 1e-6);
    CHECK_NEAR(energy.cpu.wattHours, 100.0, 1e-6);
    CHECK_NEAR(energy.totalWattHours, 400.0, 1e-6);
    CHECK_NEAR(energy.fpsPerWatt, 0.3, 1e-9);
    CHECK_NEAR(energy.millijoulesPerFrame, 400.0 * 3600.0 * 1000.0 / 432000.0, 1e-6);
}

TEST_CASE("energy: missing sensor is reported as not measured") {
    std::vector<session::SensorSample> samples(10);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i].timeSec = static_cast<double>(i);
        samples[i].gpuPower = 200.0f;
    }
    const auto energy = analyzeEnergy(samples, 60.0, 600);
    CHECK(energy.gpu.measured);
    CHECK(!energy.cpu.measured);
    CHECK_NEAR(energy.cpu.wattHours, 0.0, 1e-12);
}
