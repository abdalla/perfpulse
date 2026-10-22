#pragma once

#include <vector>

#include "analysis/bottleneck_analyzer.hpp"
#include "analysis/energy_analyzer.hpp"
#include "analysis/frame_statistics.hpp"
#include "analysis/smoothness_score.hpp"
#include "analysis/stutter_detector.hpp"
#include "session/session_data.hpp"

namespace perfpulse::analysis {

// Downsampled frametime envelope for the frametime chart.
struct FrametimeBucket {
    double timeSec = 0.0;
    float minMs = 0.0f;
    float avgMs = 0.0f;
    float maxMs = 0.0f;
};

// Summary of a sensor channel over the active (foreground) part of the session.
struct ChannelSummary {
    bool available = false;
    double average = 0.0;
    double median = 0.0;
    double min = 0.0;
    double max = 0.0;
    double p95 = 0.0;
};

struct StutterContext {
    StutterEvent event;
    Bottleneck bottleneck = Bottleneck::Mixed;
    session::SensorSample sample;  // Closest sensor reading.
};

struct SessionAnalysis {
    FrameStatistics frames;
    std::vector<SampleFrameStats> perSample;  // aligned with SessionData::samples
    std::vector<FrametimeBucket> frametimeEnvelope;
    std::vector<StutterContext> stutters;
    SmoothnessScore smoothness;
    BottleneckSummary bottleneck;
    EnergySummary energy;
    double foregroundSec = 0.0;
    double backgroundSec = 0.0;
};

SessionAnalysis analyzeSession(const session::SessionData& data);

// FPS and worst frametime inside each sensor sample's interval.
std::vector<SampleFrameStats> computePerSampleStats(const session::SessionData& data);

// Builds the frametime intervals that count for statistics: game in the
// foreground, not on a loading screen, and no pause gaps.
std::vector<FrameInterval> activeFrameIntervals(const session::SessionData& data,
                                                const std::vector<SampleFrameStats>& perSample);
std::vector<FrameInterval> activeFrameIntervals(const session::SessionData& data);

template <typename Getter>
ChannelSummary summarizeChannel(const std::vector<session::SensorSample>& samples, Getter getter);

}  // namespace perfpulse::analysis

#include "analysis/session_analysis_inl.hpp"
