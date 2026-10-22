#pragma once

#include <algorithm>
#include <numeric>

// Template implementations for session_analysis.hpp.

namespace perfpulse::analysis {

template <typename Getter>
ChannelSummary summarizeChannel(const std::vector<session::SensorSample>& samples, Getter getter) {
    ChannelSummary summary;
    std::vector<float> values;
    values.reserve(samples.size());
    for (const auto& sample : samples) {
        if (!sample.gameInForeground) {
            continue;
        }
        const float value = getter(sample);
        if (session::hasValue(value)) {
            values.push_back(value);
        }
    }
    if (values.empty()) {
        return summary;
    }
    std::sort(values.begin(), values.end());
    summary.available = true;
    summary.average = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    summary.median = percentileOfSorted(values, 50.0);
    summary.p95 = percentileOfSorted(values, 95.0);
    summary.min = values.front();
    summary.max = values.back();
    return summary;
}

}  // namespace perfpulse::analysis
