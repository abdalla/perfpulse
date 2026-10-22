#pragma once

#include <cstddef>
#include <vector>

namespace perfpulse::analysis {

// Interval between two consecutive presents, timestamped at the later one.
struct FrameInterval {
    double timeSec = 0.0;
    float ms = 0.0f;
};

struct HistogramBin {
    float fromMs = 0.0f;
    float toMs = 0.0f;
    size_t count = 0;
};

struct PercentilePoint {
    double percentile = 0.0;  // 0..100
    double ms = 0.0;
};

struct FrameStatistics {
    size_t frameCount = 0;
    double activeSec = 0.0;

    double averageFps = 0.0;           // frames / time, the "real" average
    double medianFps = 0.0;            // 1000 / median frametime
    double onePercentLowFps = 0.0;     // 1000 / 99th percentile frametime
    double pointOnePercentLowFps = 0.0;// 1000 / 99.9th percentile frametime
    double onePercentLowAverageFps = 0.0;  // average of the slowest 1% of frames

    double averageFrametimeMs = 0.0;
    double medianFrametimeMs = 0.0;
    double p95FrametimeMs = 0.0;
    double p99FrametimeMs = 0.0;
    double p999FrametimeMs = 0.0;
    double maxFrametimeMs = 0.0;
    double stdDevFrametimeMs = 0.0;
    // Mean absolute difference between consecutive frames, relative to the
    // mean frametime. 0 = perfectly even pacing.
    double relativeJitter = 0.0;
    // "Local 1% low": 1 / p99 of (frametime / median of its 1 s window).
    // Unlike 1% low / average, it ignores scenes that are simply heavier and
    // only measures how uneven frames are relative to their neighbours.
    double localLowRatio = 1.0;

    std::vector<HistogramBin> histogram;
    std::vector<PercentilePoint> percentileCurve;
};

// Gaps longer than this are treated as pauses (loading screens, alt-tab)
// rather than frames.
inline constexpr float kMaxFrameGapMs = 1000.0f;

// Sensor intervals below this FPS are loading screens/transitions and are
// left out of the frame statistics.
inline constexpr float kLoadingFps = 8.0f;

FrameStatistics computeFrameStatistics(const std::vector<FrameInterval>& intervals);

// Value at the given percentile (0..100) of an already sorted vector.
double percentileOfSorted(const std::vector<float>& sorted, double percentile);

}  // namespace perfpulse::analysis
