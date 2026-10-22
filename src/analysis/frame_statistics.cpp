#include "analysis/frame_statistics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace perfpulse::analysis {

namespace {

std::vector<HistogramBin> buildHistogram(const std::vector<float>& sorted) {
    std::vector<HistogramBin> bins;
    if (sorted.empty()) {
        return bins;
    }
    // Bin width adapts to the frame rate: ~0.25 ms at 240 FPS, ~1 ms at 60 FPS.
    const double median = percentileOfSorted(sorted, 50.0);
    const double width = std::clamp(std::round(median / 16.0 * 4.0) / 4.0, 0.25, 4.0);
    const double upper = std::min(percentileOfSorted(sorted, 99.9) * 1.1, median * 6.0);
    const double lower = std::max(0.0, std::floor(sorted.front() / width) * width);
    const auto binCount = static_cast<size_t>(std::clamp((upper - lower) / width + 1.0, 1.0, 160.0));
    bins.resize(binCount);
    for (size_t i = 0; i < binCount; ++i) {
        bins[i].fromMs = static_cast<float>(lower + width * static_cast<double>(i));
        bins[i].toMs = static_cast<float>(lower + width * static_cast<double>(i + 1));
    }
    for (const float ms : sorted) {
        auto index = static_cast<size_t>(std::max(0.0, (ms - lower) / width));
        bins[std::min(index, binCount - 1)].count++;
    }
    return bins;
}

double computeLocalLowRatio(const std::vector<FrameInterval>& intervals) {
    std::vector<float> ratios;
    ratios.reserve(intervals.size());
    std::vector<float> window;
    size_t begin = 0;
    while (begin < intervals.size()) {
        const double windowEnd = std::floor(intervals[begin].timeSec) + 1.0;
        size_t end = begin;
        while (end < intervals.size() && intervals[end].timeSec < windowEnd) {
            ++end;
        }
        window.clear();
        for (size_t i = begin; i < end; ++i) {
            window.push_back(intervals[i].ms);
        }
        const auto middle = window.begin() + static_cast<ptrdiff_t>(window.size() / 2);
        std::nth_element(window.begin(), middle, window.end());
        const float median = *middle;
        if (median > 0.0f) {
            for (size_t i = begin; i < end; ++i) {
                ratios.push_back(intervals[i].ms / median);
            }
        }
        begin = end;
    }
    if (ratios.empty()) {
        return 1.0;
    }
    std::sort(ratios.begin(), ratios.end());
    const double p99 = percentileOfSorted(ratios, 99.0);
    return p99 > 0.0 ? std::min(1.0, 1.0 / p99) : 1.0;
}

std::vector<PercentilePoint> buildPercentileCurve(const std::vector<float>& sorted) {
    static constexpr double kPoints[] = {0,  1,  5,  10, 20, 30, 40, 50,   60,   70,   80,  90,
                                         95, 97, 98, 99, 99.5, 99.8, 99.9, 99.95, 99.99, 100};
    std::vector<PercentilePoint> curve;
    if (sorted.empty()) {
        return curve;
    }
    for (const double p : kPoints) {
        curve.push_back({p, percentileOfSorted(sorted, p)});
    }
    return curve;
}

}  // namespace

double percentileOfSorted(const std::vector<float>& sorted, double percentile) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double rank = std::clamp(percentile, 0.0, 100.0) / 100.0 * static_cast<double>(sorted.size() - 1);
    const auto lowerIndex = static_cast<size_t>(std::floor(rank));
    const auto upperIndex = std::min(lowerIndex + 1, sorted.size() - 1);
    const double fraction = rank - static_cast<double>(lowerIndex);
    return sorted[lowerIndex] + (sorted[upperIndex] - sorted[lowerIndex]) * fraction;
}

FrameStatistics computeFrameStatistics(const std::vector<FrameInterval>& intervals) {
    FrameStatistics stats;
    if (intervals.empty()) {
        return stats;
    }

    std::vector<float> sorted;
    sorted.reserve(intervals.size());
    double totalMs = 0.0;
    double jitterSum = 0.0;
    for (size_t i = 0; i < intervals.size(); ++i) {
        sorted.push_back(intervals[i].ms);
        totalMs += intervals[i].ms;
        if (i > 0) {
            jitterSum += std::fabs(intervals[i].ms - intervals[i - 1].ms);
        }
    }
    std::sort(sorted.begin(), sorted.end());

    const auto count = static_cast<double>(sorted.size());
    stats.frameCount = sorted.size();
    stats.activeSec = totalMs / 1000.0;
    stats.averageFrametimeMs = totalMs / count;
    stats.averageFps = 1000.0 / stats.averageFrametimeMs;
    stats.medianFrametimeMs = percentileOfSorted(sorted, 50.0);
    stats.p95FrametimeMs = percentileOfSorted(sorted, 95.0);
    stats.p99FrametimeMs = percentileOfSorted(sorted, 99.0);
    stats.p999FrametimeMs = percentileOfSorted(sorted, 99.9);
    stats.maxFrametimeMs = sorted.back();
    stats.medianFps = 1000.0 / stats.medianFrametimeMs;
    stats.onePercentLowFps = 1000.0 / stats.p99FrametimeMs;
    stats.pointOnePercentLowFps = 1000.0 / stats.p999FrametimeMs;

    const auto slowestCount = std::max<size_t>(1, sorted.size() / 100);
    const double slowestSum = std::accumulate(sorted.end() - static_cast<ptrdiff_t>(slowestCount), sorted.end(), 0.0);
    stats.onePercentLowAverageFps = 1000.0 / (slowestSum / static_cast<double>(slowestCount));

    double variance = 0.0;
    for (const float ms : sorted) {
        const double delta = ms - stats.averageFrametimeMs;
        variance += delta * delta;
    }
    stats.stdDevFrametimeMs = std::sqrt(variance / count);
    stats.relativeJitter =
        sorted.size() > 1 ? (jitterSum / (count - 1.0)) / stats.averageFrametimeMs : 0.0;

    stats.localLowRatio = computeLocalLowRatio(intervals);
    stats.histogram = buildHistogram(sorted);
    stats.percentileCurve = buildPercentileCurve(sorted);
    return stats;
}

}  // namespace perfpulse::analysis
