#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "core/json.hpp"
#include "session/session_data.hpp"

namespace perfpulse::report {

// Marker inside the HTML templates replaced with the JSON payload.
inline constexpr std::string_view kDataPlaceholder = "__PERFPULSE_DATA__";

// Injects the payload into a template. Returns nullopt if the template has no
// placeholder.
std::optional<std::string> renderTemplate(std::string_view templateHtml, const core::Json& payload);

// "PerfPulse_2026-10-22_2130_Cyberpunk_2077.html", using local time.
std::string reportFileName(const session::SessionInfo& info);

// "2026-10-22 21:30" in local time.
std::string formatLocalDateTime(int64_t unixMs);

}  // namespace perfpulse::report
