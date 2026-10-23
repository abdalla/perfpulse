#pragma once

#include <optional>
#include <string_view>

#include "core/json.hpp"

namespace perfpulse::report {

// Pulls the JSON payload back out of a generated report.
std::optional<core::Json> extractPayload(std::string_view html);

// Re-runs the bottleneck analysis of an existing report with the current
// rules. Reports keep the per-second sensor series but not the raw frames,
// so frame statistics, stutters and the grade are kept as they were; the
// bottleneck section, the timeline and each stutter's context are rebuilt.
// Returns nullopt when the payload has no usable series.
std::optional<core::Json> reanalyzePayload(const core::Json& payload);

}  // namespace perfpulse::report
