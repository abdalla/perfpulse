#include "analysis/stutter_detector.hpp"

#include <algorithm>
#include <cmath>

namespace perfpulse::analysis {

std::vector<StutterEvent> detectStutters(const std::vector<FrameInterval>& intervals,
                                         const StutterOptions& options) {
    std::vector<StutterEvent> events;
    if (intervals.size() < 8) {
        return events;
    }

    std::vector<float> window;
    size_t begin = 0;
    while (begin < intervals.size()) {
        const double windowStart = std::floor(intervals[begin].timeSec / options.windowSec) * options.windowSec;
        const double windowEnd = windowStart + options.windowSec;
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
        const double median = *middle;
        const double threshold =
            std::max({median * options.relativeThreshold, median + options.absoluteMarginMs, options.floorMs});

        for (size_t i = begin; i < end; ++i) {
            const auto& frame = intervals[i];
            if (frame.ms < threshold) {
                continue;
            }
            if (!events.empty() && frame.timeSec - events.back().timeSec <= options.mergeWindowSec) {
                auto& last = events.back();
                last.framesInvolved++;
                if (frame.ms > last.frametimeMs) {
                    last.frametimeMs = frame.ms;
                    last.timeSec = frame.timeSec;
                }
                continue;
            }
            events.push_back({frame.timeSec, frame.ms, static_cast<float>(median), 1});
        }
        begin = end;
    }
    return events;
}

}  // namespace perfpulse::analysis
