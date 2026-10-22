#include "analysis/session_analysis.hpp"

#include <algorithm>
#include <cmath>

namespace perfpulse::analysis {

namespace {

using session::SensorSample;
using session::SessionData;

// A sample at time t describes the interval (t - interval, t].
size_t sampleIndexAt(const SessionData& data, double timeSec) {
    const auto& samples = data.samples;
    const auto it = std::lower_bound(samples.begin(), samples.end(), timeSec,
                                     [](const SensorSample& s, double t) { return s.timeSec < t; });
    if (it == samples.end()) {
        return samples.empty() ? 0 : samples.size() - 1;
    }
    return static_cast<size_t>(it - samples.begin());
}

std::vector<FrametimeBucket> buildEnvelope(const std::vector<FrameInterval>& intervals, double durationSec);

}  // namespace

std::vector<SampleFrameStats> computePerSampleStats(const SessionData& data) {
    std::vector<SampleFrameStats> result(data.samples.size());
    const auto& frames = data.frameTimesSec;
    size_t frameIndex = 0;
    double previousTime = 0.0;
    for (size_t i = 0; i < data.samples.size(); ++i) {
        const double end = data.samples[i].timeSec;
        const double start = i == 0 ? std::max(0.0, end - data.sampleIntervalSec) : previousTime;
        previousTime = end;
        while (frameIndex < frames.size() && frames[frameIndex] <= start) {
            ++frameIndex;
        }
        size_t count = 0;
        float maxMs = 0.0f;
        while (frameIndex < frames.size() && frames[frameIndex] <= end) {
            if (frameIndex > 0) {
                const auto ms = static_cast<float>((frames[frameIndex] - frames[frameIndex - 1]) * 1000.0);
                if (ms < kMaxFrameGapMs) {
                    maxMs = std::max(maxMs, ms);
                }
            }
            ++count;
            ++frameIndex;
        }
        const double span = end - start;
        result[i].fps = span > 0.0 ? static_cast<float>(static_cast<double>(count) / span) : 0.0f;
        result[i].maxFrametimeMs = maxMs;
    }
    return result;
}

namespace {

std::vector<FrametimeBucket> buildEnvelope(const std::vector<FrameInterval>& intervals, double durationSec) {
    std::vector<FrametimeBucket> buckets;
    if (intervals.empty()) {
        return buckets;
    }
    // Keep the chart under ~4000 points regardless of session length.
    const double width = std::max(0.1, durationSec / 4000.0);
    FrametimeBucket current;
    double bucketEnd = 0.0;
    double sum = 0.0;
    size_t count = 0;
    for (const auto& frame : intervals) {
        if (count > 0 && frame.timeSec >= bucketEnd) {
            current.avgMs = static_cast<float>(sum / static_cast<double>(count));
            buckets.push_back(current);
            count = 0;
        }
        if (count == 0) {
            const double start = std::floor(frame.timeSec / width) * width;
            bucketEnd = start + width;
            current = {start + width / 2.0, frame.ms, 0.0f, frame.ms};
            sum = 0.0;
        }
        current.minMs = std::min(current.minMs, frame.ms);
        current.maxMs = std::max(current.maxMs, frame.ms);
        sum += frame.ms;
        ++count;
    }
    if (count > 0) {
        current.avgMs = static_cast<float>(sum / static_cast<double>(count));
        buckets.push_back(current);
    }
    return buckets;
}

}  // namespace

std::vector<FrameInterval> activeFrameIntervals(const SessionData& data) {
    return activeFrameIntervals(data, computePerSampleStats(data));
}

std::vector<FrameInterval> activeFrameIntervals(const SessionData& data,
                                                const std::vector<SampleFrameStats>& perSample) {
    std::vector<FrameInterval> intervals;
    const auto& frames = data.frameTimesSec;
    if (frames.size() < 2) {
        return intervals;
    }
    intervals.reserve(frames.size());
    size_t sampleIndex = 0;
    for (size_t i = 1; i < frames.size(); ++i) {
        const auto ms = static_cast<float>((frames[i] - frames[i - 1]) * 1000.0);
        if (ms <= 0.0f || ms >= kMaxFrameGapMs) {
            continue;
        }
        while (sampleIndex + 1 < data.samples.size() && data.samples[sampleIndex].timeSec < frames[i]) {
            ++sampleIndex;
        }
        if (!data.samples.empty() && !data.samples[sampleIndex].gameInForeground) {
            continue;
        }
        if (sampleIndex < perSample.size() && perSample[sampleIndex].fps < kLoadingFps) {
            continue;
        }
        intervals.push_back({frames[i], ms});
    }
    return intervals;
}

SessionAnalysis analyzeSession(const SessionData& data) {
    SessionAnalysis analysis;
    analysis.perSample = computePerSampleStats(data);
    const auto intervals = activeFrameIntervals(data, analysis.perSample);
    analysis.frames = computeFrameStatistics(intervals);
    analysis.frametimeEnvelope = buildEnvelope(intervals, data.durationSec);
    analysis.bottleneck = analyzeBottlenecks(data.samples, analysis.perSample, data.info.monitorRefreshHz,
                                             data.info.hardware.cpuTjMax);

    const auto stutters = detectStutters(intervals);
    analysis.stutters.reserve(stutters.size());
    for (const auto& event : stutters) {
        StutterContext context;
        context.event = event;
        if (!data.samples.empty()) {
            const size_t index = sampleIndexAt(data, event.timeSec);
            context.sample = data.samples[index];
            if (index < analysis.bottleneck.timeline.size()) {
                context.bottleneck = analysis.bottleneck.timeline[index];
            }
        }
        analysis.stutters.push_back(context);
    }

    analysis.smoothness = computeSmoothnessScore(analysis.frames, stutters.size());
    analysis.energy = analyzeEnergy(data.samples, analysis.frames.averageFps, analysis.frames.frameCount);

    double previous = 0.0;
    for (const auto& sample : data.samples) {
        const double dt = sample.timeSec - previous;
        previous = sample.timeSec;
        (sample.gameInForeground ? analysis.foregroundSec : analysis.backgroundSec) += dt;
    }
    return analysis;
}

}  // namespace perfpulse::analysis
