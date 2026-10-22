#include "analysis/bottleneck_analyzer.hpp"
#include "test_framework.hpp"

using namespace perfpulse;
using namespace perfpulse::analysis;

namespace {

session::SensorSample makeSample(float gpu, float thread, float cpu = 30.0f) {
    session::SensorSample s;
    s.gpuUsage = gpu;
    s.gameBusiestThread = thread;
    s.cpuUsage = cpu;
    s.cpuBusiestCore = thread;
    s.vramUsedGb = 6.0f;
    s.vramTotalGb = 12.0f;
    s.ramUsedGb = 16.0f;
    s.ramTotalGb = 64.0f;
    s.gpuLimiters = session::kLimiterPower;
    return s;
}

}  // namespace

TEST_CASE("bottleneck: saturated GPU is the offender") {
    std::vector<session::SensorSample> samples(60, makeSample(98.0f, 60.0f));
    std::vector<SampleFrameStats> frames(60, {110.0f, 12.0f});
    for (size_t i = 0; i < frames.size(); ++i) {
        frames[i].fps = 100.0f + static_cast<float>(i % 17);  // no plateau
    }
    const auto summary = analyzeBottlenecks(samples, frames, 144, 100.0f);
    CHECK(summary.offender == Bottleneck::Gpu);
    CHECK_NEAR(summary.offenderShare, 1.0, 1e-9);
    CHECK_NEAR(summary.gpuPowerLimitedShare, 1.0, 1e-9);
}

TEST_CASE("bottleneck: idle GPU and saturated thread is CPU-bound") {
    std::vector<session::SensorSample> samples(60, makeSample(62.0f, 97.0f));
    std::vector<SampleFrameStats> frames(60);
    for (size_t i = 0; i < frames.size(); ++i) {
        frames[i].fps = 70.0f + static_cast<float>(i % 23);
    }
    const auto summary = analyzeBottlenecks(samples, frames, 144, 100.0f);
    CHECK(summary.offender == Bottleneck::Cpu);
    CHECK_NEAR(summary.cpuBoundBusiestThread, 97.0, 1e-3);
}

TEST_CASE("bottleneck: plateau on refresh rate is a frame cap") {
    std::vector<session::SensorSample> samples(60, makeSample(55.0f, 50.0f));
    std::vector<SampleFrameStats> frames(60, {143.6f, 7.2f});
    const auto summary = analyzeBottlenecks(samples, frames, 144, 100.0f);
    CHECK(summary.offender == Bottleneck::FrameCap);
    CHECK_NEAR(summary.detectedCapFps, 144.0, 1e-6);
    CHECK(summary.uncappedEstimateFps > 200.0f);
}

TEST_CASE("bottleneck: background and loading are excluded from shares") {
    std::vector<session::SensorSample> samples(10, makeSample(98.0f, 50.0f));
    samples[0].gameInForeground = false;
    std::vector<SampleFrameStats> frames(10, {90.0f, 12.0f});
    frames[1].fps = 2.0f;
    const auto summary = analyzeBottlenecks(samples, frames, 60, 100.0f);
    CHECK(summary.timeline[0] == Bottleneck::Background);
    CHECK(summary.timeline[1] == Bottleneck::Loading);
    CHECK_NEAR(summary.shareOfActiveTime[static_cast<size_t>(Bottleneck::Gpu)], 1.0, 1e-9);
}

TEST_CASE("bottleneck: full VRAM wins over GPU usage") {
    auto sample = makeSample(99.0f, 50.0f);
    sample.vramUsedGb = 11.9f;
    std::vector<session::SensorSample> samples(30, sample);
    std::vector<SampleFrameStats> frames(30);
    for (size_t i = 0; i < frames.size(); ++i) {
        frames[i].fps = 50.0f + static_cast<float>(i);
    }
    CHECK(analyzeBottlenecks(samples, frames, 60, 100.0f).offender == Bottleneck::Vram);
}
