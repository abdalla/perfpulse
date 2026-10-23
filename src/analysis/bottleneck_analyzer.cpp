#include "analysis/bottleneck_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace perfpulse::analysis {

namespace {

using session::hasValue;
using session::SensorSample;

bool isActive(Bottleneck b) {
    return b != Bottleneck::Background && b != Bottleneck::Loading;
}

bool isAtCap(float fps, float capFps, const BottleneckThresholds& t) {
    return capFps > 0.0f && std::fabs(fps - capFps) <= capFps * t.capTolerance;
}

// Looks for a FPS plateau: a value most active samples sit on while the GPU
// is not saturated. That is the signature of a frame limiter or V-Sync.
float detectCapPlateau(const std::vector<SensorSample>& samples, const std::vector<SampleFrameStats>& frames,
                       int refreshRateHz, const BottleneckThresholds& t) {
    std::map<int, int> histogram;
    int considered = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        const float fps = frames[i].fps;
        if (!s.gameInForeground || fps < t.loadingFps) {
            continue;
        }
        ++considered;
        if (hasValue(s.gpuUsage) && s.gpuUsage >= t.gpuSaturated) {
            continue;
        }
        histogram[static_cast<int>(std::lround(fps))]++;
    }
    if (considered == 0) {
        return 0.0f;
    }

    float bestCap = 0.0f;
    int bestCount = 0;
    for (const auto& [value, count] : histogram) {
        // Sum neighbours within the tolerance band around this value.
        int band = 0;
        const int radius = std::max(1, static_cast<int>(std::lround(value * t.capTolerance)));
        for (auto it = histogram.lower_bound(value - radius); it != histogram.end() && it->first <= value + radius;
             ++it) {
            band += it->second;
        }
        if (band > bestCount) {
            bestCount = band;
            bestCap = static_cast<float>(value);
        }
    }
    if (bestCount < std::max(5, static_cast<int>(static_cast<float>(considered) * t.capMinShare))) {
        return 0.0f;
    }
    // Report the median of the plateau, not the centre of the best band:
    // limiters sit slightly below their nominal value (Reflex: 171 on 180 Hz).
    const int radius = std::max(1, static_cast<int>(std::lround(bestCap * t.capTolerance)));
    const auto first = histogram.lower_bound(static_cast<int>(bestCap) - radius);
    int seen = 0;
    for (auto it = first; it != histogram.end() && it->first <= static_cast<int>(bestCap) + radius; ++it) {
        seen += it->second;
        if (seen * 2 >= bestCount) {
            bestCap = static_cast<float>(it->first);
            break;
        }
    }
    // Snap to the refresh rate when the plateau sits on it (V-Sync / G-Sync cap).
    if (refreshRateHz > 0 && std::fabs(bestCap - static_cast<float>(refreshRateHz)) <=
                                 static_cast<float>(refreshRateHz) * t.capTolerance) {
        return static_cast<float>(refreshRateHz);
    }
    return bestCap;
}

Bottleneck classify(const SensorSample& s, const SampleFrameStats& frame, float capFps, float cpuTjMax,
                    const BottleneckThresholds& t) {
    if (!s.gameInForeground) {
        return Bottleneck::Background;
    }
    if (frame.fps < t.loadingFps) {
        return Bottleneck::Loading;
    }
    if (hasValue(s.vramUsedGb) && hasValue(s.vramTotalGb) && s.vramTotalGb > 0.0f &&
        s.vramUsedGb / s.vramTotalGb >= t.vramFull) {
        return Bottleneck::Vram;
    }
    if (hasValue(s.ramUsedGb) && hasValue(s.ramTotalGb) && s.ramTotalGb > 0.0f &&
        s.ramUsedGb / s.ramTotalGb >= t.ramFull) {
        return Bottleneck::Ram;
    }

    const bool gpuKnown = hasValue(s.gpuUsage);
    const bool gpuSaturated = gpuKnown && s.gpuUsage >= t.gpuSaturated;
    const bool gpuWaiting = gpuKnown && s.gpuUsage < t.gpuIdleForCpu;
    const bool threadSaturated = hasValue(s.gameBusiestThread) && s.gameBusiestThread >= t.threadSaturated;
    const bool coreSaturated = hasValue(s.cpuBusiestCore) && s.cpuBusiestCore >= t.coreSaturated;
    const bool cpuSaturated = hasValue(s.cpuUsage) && s.cpuUsage >= t.cpuTotalSaturated;

    // At the cap the limiter decides the frame rate, whatever else is busy: a
    // saturated thread there is often the limiter itself spin-waiting.
    if (isAtCap(frame.fps, capFps, t)) {
        return Bottleneck::FrameCap;
    }
    if (gpuSaturated) {
        const bool throttled = s.gpuLimiters != session::kLimiterUnknown &&
                               (s.gpuLimiters & (session::kLimiterThermal | session::kLimiterHardwareSlowdown));
        return throttled ? Bottleneck::GpuThrottling : Bottleneck::Gpu;
    }
    if (gpuWaiting && (threadSaturated || coreSaturated || cpuSaturated)) {
        const bool hot = hasValue(s.cpuTemperature) && hasValue(cpuTjMax) &&
                         s.cpuTemperature >= cpuTjMax - t.cpuThermalMarginC;
        return hot ? Bottleneck::CpuThermal : Bottleneck::Cpu;
    }
    if (!gpuKnown && (threadSaturated || cpuSaturated)) {
        return Bottleneck::Cpu;
    }
    return Bottleneck::Mixed;
}

}  // namespace

std::string_view bottleneckKey(Bottleneck bottleneck) {
    switch (bottleneck) {
        case Bottleneck::Gpu: return "gpu";
        case Bottleneck::GpuThrottling: return "gpu_throttling";
        case Bottleneck::Cpu: return "cpu";
        case Bottleneck::CpuThermal: return "cpu_thermal";
        case Bottleneck::Vram: return "vram";
        case Bottleneck::Ram: return "ram";
        case Bottleneck::FrameCap: return "frame_cap";
        case Bottleneck::Mixed: return "mixed";
        case Bottleneck::Loading: return "loading";
        case Bottleneck::Background: return "background";
        case Bottleneck::Count: break;
    }
    return "mixed";
}

BottleneckSummary analyzeBottlenecks(const std::vector<SensorSample>& samples,
                                     const std::vector<SampleFrameStats>& frameStats, int refreshRateHz,
                                     float cpuTjMax, const BottleneckThresholds& thresholds) {
    BottleneckSummary summary;
    const size_t count = std::min(samples.size(), frameStats.size());
    if (count == 0) {
        return summary;
    }

    summary.detectedCapFps = detectCapPlateau(samples, frameStats, refreshRateHz, thresholds);
    summary.timeline.reserve(count);

    std::array<size_t, kBottleneckCount> counts{};
    size_t activeCount = 0;
    double cappedGpuUsageSum = 0.0;
    double cappedFpsSum = 0.0;
    size_t cappedWithGpu = 0;
    double cappedThreadSum = 0.0;
    size_t cappedWithThread = 0;
    size_t cappedThreadSaturated = 0;
    size_t gpuThermalLimited = 0;
    double cpuThreadSum = 0.0;
    size_t cpuThreadCount = 0;
    double cpuGpuSum = 0.0;
    size_t cpuGpuCount = 0;
    size_t gpuBound = 0;
    size_t gpuPowerLimited = 0;

    for (size_t i = 0; i < count; ++i) {
        const auto& s = samples[i];
        const auto b = classify(s, frameStats[i], summary.detectedCapFps, cpuTjMax, thresholds);
        summary.timeline.push_back(b);
        counts[static_cast<size_t>(b)]++;
        if (!isActive(b)) {
            continue;
        }
        ++activeCount;
        // Reported whatever the classification: at its temperature target the
        // GPU lowers clocks even when it is not the component limiting FPS.
        if (s.gpuLimiters != session::kLimiterUnknown && (s.gpuLimiters & session::kLimiterThermal)) {
            ++gpuThermalLimited;
        }
        if (b == Bottleneck::FrameCap) {
            if (hasValue(s.gpuUsage) && s.gpuUsage > 1.0f) {
                cappedGpuUsageSum += s.gpuUsage;
                cappedFpsSum += frameStats[i].fps;
                ++cappedWithGpu;
            }
            if (hasValue(s.gameBusiestThread)) {
                cappedThreadSum += s.gameBusiestThread;
                ++cappedWithThread;
                cappedThreadSaturated += s.gameBusiestThread >= thresholds.threadSaturated ? 1 : 0;
            }
        }
        if (b == Bottleneck::Cpu || b == Bottleneck::CpuThermal) {
            if (hasValue(s.gameBusiestThread)) {
                cpuThreadSum += s.gameBusiestThread;
                ++cpuThreadCount;
            }
            if (hasValue(s.gpuUsage)) {
                cpuGpuSum += s.gpuUsage;
                ++cpuGpuCount;
            }
        }
        if (b == Bottleneck::Gpu || b == Bottleneck::GpuThrottling) {
            ++gpuBound;
            if (s.gpuLimiters != session::kLimiterUnknown && (s.gpuLimiters & session::kLimiterPower)) {
                ++gpuPowerLimited;
            }
        }
    }

    if (activeCount > 0) {
        for (size_t i = 0; i < kBottleneckCount; ++i) {
            if (isActive(static_cast<Bottleneck>(i))) {
                summary.shareOfActiveTime[i] = static_cast<double>(counts[i]) / static_cast<double>(activeCount);
            }
        }
    }

    // The offender is the limiting resource that dominated. A cap is only the
    // verdict when it dominates the whole session: then there is no bottleneck.
    static constexpr Bottleneck kCandidates[] = {Bottleneck::Gpu,  Bottleneck::GpuThrottling, Bottleneck::Cpu,
                                                 Bottleneck::CpuThermal, Bottleneck::Vram, Bottleneck::Ram};
    double best = -1.0;
    for (const auto candidate : kCandidates) {
        const double share = summary.shareOfActiveTime[static_cast<size_t>(candidate)];
        if (share > best) {
            best = share;
            summary.offender = candidate;
        }
    }
    summary.offenderShare = std::max(best, 0.0);
    const double capShare = summary.shareOfActiveTime[static_cast<size_t>(Bottleneck::FrameCap)];
    const double mixedShare = summary.shareOfActiveTime[static_cast<size_t>(Bottleneck::Mixed)];
    if (capShare >= 0.5 || (capShare > summary.offenderShare && capShare >= mixedShare)) {
        summary.offender = Bottleneck::FrameCap;
        summary.offenderShare = capShare;
    } else if (mixedShare > summary.offenderShare && mixedShare >= 0.4) {
        summary.offender = Bottleneck::Mixed;
        summary.offenderShare = mixedShare;
    }

    if (cappedWithThread > 0) {
        summary.cappedBusiestThread = static_cast<float>(cappedThreadSum / static_cast<double>(cappedWithThread));
        summary.cappedThreadSaturatedShare =
            static_cast<double>(cappedThreadSaturated) / static_cast<double>(cappedWithThread);
    }
    if (cappedWithGpu > 0) {
        const double avgGpu = cappedGpuUsageSum / static_cast<double>(cappedWithGpu);
        const double avgFps = cappedFpsSum / static_cast<double>(cappedWithGpu);
        summary.cappedGpuUsage = static_cast<float>(avgGpu);
        // Linear extrapolation of the headroom of each side: the GPU up to ~97%
        // usage, the game's busiest thread up to a full core. The smaller wins.
        // Optimistic by design; flagged as an estimate in the report.
        const double gpuFactor = 97.0 / avgGpu;
        double cpuFactor = 4.0;
        if (hasValue(summary.cappedBusiestThread) && summary.cappedBusiestThread > 1.0f) {
            cpuFactor = std::max(1.0, 100.0 / summary.cappedBusiestThread);
        }
        summary.uncappedLimitedBy = cpuFactor < gpuFactor ? Bottleneck::Cpu : Bottleneck::Gpu;
        summary.uncappedEstimateFps = static_cast<float>(avgFps * std::min({gpuFactor, cpuFactor, 4.0}));
    }
    if (activeCount > 0) {
        summary.gpuThermalLimitedShare = static_cast<double>(gpuThermalLimited) / static_cast<double>(activeCount);
    }
    if (cpuThreadCount > 0) {
        summary.cpuBoundBusiestThread = static_cast<float>(cpuThreadSum / static_cast<double>(cpuThreadCount));
    }
    if (cpuGpuCount > 0) {
        summary.cpuBoundGpuUsage = static_cast<float>(cpuGpuSum / static_cast<double>(cpuGpuCount));
    }
    if (gpuBound > 0) {
        summary.gpuPowerLimitedShare = static_cast<double>(gpuPowerLimited) / static_cast<double>(gpuBound);
    }
    return summary;
}

}  // namespace perfpulse::analysis
