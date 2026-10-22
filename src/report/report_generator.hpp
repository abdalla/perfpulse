#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "core/config.hpp"
#include "session/session_data.hpp"

namespace perfpulse::report {

struct GeneratedReport {
    std::filesystem::path path;
    std::string gameName;
    double averageFps = 0.0;
    std::string grade;
    double durationSec = 0.0;
};

// Analyzes the session, writes the self-contained HTML report, appends the
// session to the history and refreshes index.html. Heavy work: call it off
// the UI thread.
std::optional<GeneratedReport> generateReport(const session::SessionData& data, const core::Config& config);

// Path of index.html in the configured report folder.
std::filesystem::path historyIndexPath(const core::Config& config);

}  // namespace perfpulse::report
