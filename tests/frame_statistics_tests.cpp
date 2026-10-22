#include "analysis/frame_statistics.hpp"
#include "analysis/session_analysis.hpp"
#include "test_framework.hpp"

using namespace perfpulse;
using namespace perfpulse::analysis;

namespace {

std::vector<FrameInterval> constantFrames(size_t count, float ms) {
    std::vector<FrameInterval> frames;
    double t = 0.0;
    for (size_t i = 0; i < count; ++i) {
        t += ms / 1000.0;
        frames.push_back({t, ms});
    }
    return frames;
}

}  // namespace

TEST_CASE("frames: constant pacing gives identical averages and lows") {
    const auto stats = computeFrameStatistics(constantFrames(1000, 10.0f));
    CHECK(stats.frameCount == 1000);
    CHECK_NEAR(stats.averageFps, 100.0, 1e-6);
    CHECK_NEAR(stats.medianFps, 100.0, 1e-6);
    CHECK_NEAR(stats.onePercentLowFps, 100.0, 1e-6);
    CHECK_NEAR(stats.relativeJitter, 0.0, 1e-9);
}

TEST_CASE("frames: 1% low follows the slow frames") {
    auto frames = constantFrames(1000, 10.0f);
    for (size_t i = 0; i < 20; ++i) {
        frames[i * 50].ms = 40.0f;  // 2% of frames are 25 FPS
    }
    const auto stats = computeFrameStatistics(frames);
    CHECK_NEAR(stats.onePercentLowFps, 25.0, 0.01);
    CHECK_NEAR(stats.medianFps, 100.0, 1e-6);
    CHECK(stats.averageFps < 100.0);
    CHECK(!stats.histogram.empty());
}

TEST_CASE("frames: percentile interpolation") {
    const std::vector<float> sorted = {1.0f, 2.0f, 3.0f, 4.0f};
    CHECK_NEAR(percentileOfSorted(sorted, 0.0), 1.0, 1e-9);
    CHECK_NEAR(percentileOfSorted(sorted, 50.0), 2.5, 1e-9);
    CHECK_NEAR(percentileOfSorted(sorted, 100.0), 4.0, 1e-9);
}

TEST_CASE("session: background time and pauses are excluded") {
    session::SessionData data;
    data.sampleIntervalSec = 1.0;
    for (int i = 1; i <= 4; ++i) {
        session::SensorSample sample;
        sample.timeSec = i;
        sample.gameInForeground = i != 3;  // alt-tab during the third second
        data.samples.push_back(sample);
    }
    for (int i = 0; i <= 400; ++i) {
        data.frameTimesSec.push_back(i / 100.0);
    }
    data.frameTimesSec.push_back(9.0);  // 5 s pause: must not count as a frame
    data.durationSec = 4.0;
    const auto intervals = activeFrameIntervals(data);
    CHECK(intervals.size() == 300);
    for (const auto& interval : intervals) {
        CHECK(interval.timeSec <= 2.0 + 1e-9 || interval.timeSec > 3.0 - 1e-9);
    }
}
