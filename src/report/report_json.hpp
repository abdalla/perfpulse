#pragma once

#include <string_view>

#include "analysis/session_analysis.hpp"
#include "core/json.hpp"
#include "session/session_data.hpp"

namespace perfpulse::report {

// Builds the data payload consumed by the report template's JavaScript.
core::Json buildReportPayload(const session::SessionData& data, const analysis::SessionAnalysis& analysis,
                              std::string_view languageTag);

// The "bottleneck" section of the payload.
core::Json buildBottleneckJson(const analysis::BottleneckSummary& summary);

// One-line summary of a session, appended to the history log.
core::Json buildHistoryEntry(const session::SessionData& data, const analysis::SessionAnalysis& analysis,
                             std::string_view reportFileName);

}  // namespace perfpulse::report
