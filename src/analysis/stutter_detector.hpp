#pragma once

#include <vector>

#include "analysis/frame_statistics.hpp"

namespace perfpulse::analysis {

struct StutterEvent {
    double timeSec = 0.0;
    float frametimeMs = 0.0f;
    float localMedianMs = 0.0f;
    int framesInvolved = 1;  // Spikes merged into this event.
};

struct StutterOptions {
    double relativeThreshold = 2.5;   // spike must exceed N x local median...
    double absoluteMarginMs = 8.0;    // ...and the median plus this margin...
    double floorMs = 12.0;            // ...and this absolute floor.
    double windowSec = 1.0;           // local median window.
    double mergeWindowSec = 0.25;     // spikes closer than this form one event.
};

// Finds hitches: single frames that take much longer than the frames around
// them. Uses a per-window median so that a scene that is simply heavier is not
// reported as stutter.
std::vector<StutterEvent> detectStutters(const std::vector<FrameInterval>& intervals,
                                         const StutterOptions& options = {});

}  // namespace perfpulse::analysis
