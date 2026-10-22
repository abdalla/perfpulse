#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "core/json.hpp"

namespace perfpulse::report {

// Session history kept next to the reports as JSON Lines (one session per
// line), so appending never rewrites earlier sessions.
class HistoryStore {
public:
    explicit HistoryStore(std::filesystem::path reportDirectory);

    bool append(const core::Json& entry) const;
    // Entries whose report file still exists, oldest first.
    std::vector<core::Json> load() const;

    // Regenerates index.html from the template. Returns false on failure.
    bool writeIndex(std::string_view indexTemplate, std::string_view languageTag) const;

    std::filesystem::path historyPath() const { return directory_ / L"history.jsonl"; }
    std::filesystem::path indexPath() const { return directory_ / L"index.html"; }

private:
    std::filesystem::path directory_;
};

}  // namespace perfpulse::report
