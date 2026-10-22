#pragma once

#include <string>

#include "analysis/frame_statistics.hpp"

namespace perfpulse::analysis {

struct SmoothnessScore {
    int score = 0;        // 0..100
    std::string grade;    // "S", "A", "B", "C" or "D"
    // Components in 0..1, exposed so the report can explain the grade.
    double consistency = 0.0;  // local 1% low (frames vs. their 1 s neighbourhood)
    double stutterFree = 0.0;  // stutter events per minute
    double pacing = 0.0;       // frame-to-frame jitter
};

SmoothnessScore computeSmoothnessScore(const FrameStatistics& stats, size_t stutterCount);

}  // namespace perfpulse::analysis
