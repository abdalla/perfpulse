#include "analysis/smoothness_score.hpp"
#include "analysis/stutter_detector.hpp"
#include "test_framework.hpp"

using namespace perfpulse::analysis;

namespace {

std::vector<FrameInterval> framesWithSpikes(float baseMs, const std::vector<std::pair<size_t, float>>& spikes) {
    std::vector<FrameInterval> frames;
    double t = 0.0;
    for (size_t i = 0; i < 2000; ++i) {
        float ms = baseMs;
        for (const auto& [index, value] : spikes) {
            if (index == i) {
                ms = value;
            }
        }
        t += ms / 1000.0;
        frames.push_back({t, ms});
    }
    return frames;
}

}  // namespace

TEST_CASE("stutter: isolated spikes are detected") {
    const auto frames = framesWithSpikes(7.0f, {{300, 45.0f}, {1200, 60.0f}});
    const auto events = detectStutters(frames);
    CHECK(events.size() == 2);
    CHECK_NEAR(events[0].frametimeMs, 45.0, 1e-6);
}

TEST_CASE("stutter: small variation is not a stutter") {
    const auto frames = framesWithSpikes(16.6f, {{100, 22.0f}, {500, 30.0f}});
    CHECK(detectStutters(frames).empty());
}

TEST_CASE("stutter: adjacent spikes merge into one event") {
    const auto frames = framesWithSpikes(7.0f, {{300, 40.0f}, {302, 50.0f}, {304, 42.0f}});
    const auto events = detectStutters(frames);
    CHECK(events.size() == 1);
    CHECK(events[0].framesInvolved == 3);
    CHECK_NEAR(events[0].frametimeMs, 50.0, 1e-6);
}

TEST_CASE("score: smooth sessions grade S, stuttery ones drop") {
    FrameStatistics smooth;
    smooth.frameCount = 10000;
    smooth.activeSec = 600;
    smooth.averageFps = 144;
    smooth.onePercentLowFps = 130;
    smooth.localLowRatio = 0.92;
    smooth.relativeJitter = 0.02;
    CHECK(computeSmoothnessScore(smooth, 0).grade == "S");

    FrameStatistics rough = smooth;
    rough.localLowRatio = 0.42;
    rough.relativeJitter = 0.2;
    const auto score = computeSmoothnessScore(rough, 100);
    CHECK(score.score < 50);
    CHECK(score.grade == "D");
}
