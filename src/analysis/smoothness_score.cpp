#include "analysis/smoothness_score.hpp"

#include <algorithm>
#include <cmath>

namespace perfpulse::analysis {

SmoothnessScore computeSmoothnessScore(const FrameStatistics& stats, size_t stutterCount) {
    SmoothnessScore result;
    if (stats.frameCount == 0 || stats.averageFps <= 0.0) {
        result.grade = "-";
        return result;
    }

    // Slowest 1% of frames within 10% of their neighbours is excellent; at
    // 2.5x their neighbours (ratio 0.4) it is very uneven.
    result.consistency = std::clamp((stats.localLowRatio - 0.4) / 0.5, 0.0, 1.0);

    // Three hitches per minute already feel bad.
    const double minutes = std::max(stats.activeSec / 60.0, 1.0 / 60.0);
    const double stuttersPerMinute = static_cast<double>(stutterCount) / minutes;
    result.stutterFree = std::exp(-stuttersPerMinute / 3.0);

    // 25% average frame-to-frame variation is visibly juddery.
    result.pacing = std::clamp(1.0 - stats.relativeJitter / 0.25, 0.0, 1.0);

    const double weighted = 0.45 * result.consistency + 0.35 * result.stutterFree + 0.20 * result.pacing;
    result.score = static_cast<int>(std::lround(weighted * 100.0));
    if (result.score >= 90) {
        result.grade = "S";
    } else if (result.score >= 80) {
        result.grade = "A";
    } else if (result.score >= 65) {
        result.grade = "B";
    } else if (result.score >= 50) {
        result.grade = "C";
    } else {
        result.grade = "D";
    }
    return result;
}

}  // namespace perfpulse::analysis
