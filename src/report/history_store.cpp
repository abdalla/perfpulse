#include "report/history_store.hpp"

#include <fstream>
#include <string>

#include "core/strings.hpp"
#include "report/html_report_writer.hpp"

namespace perfpulse::report {

HistoryStore::HistoryStore(std::filesystem::path reportDirectory) : directory_(std::move(reportDirectory)) {}

bool HistoryStore::append(const core::Json& entry) const {
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    std::ofstream stream(historyPath(), std::ios::binary | std::ios::app);
    if (!stream) {
        return false;
    }
    stream << entry.dump() << '\n';
    return static_cast<bool>(stream);
}

bool HistoryStore::updateEntry(std::string_view reportFileName,
                               const std::function<void(core::Json&)>& update) const {
    std::ifstream input(historyPath(), std::ios::binary);
    if (!input) {
        return false;
    }
    std::string rewritten;
    std::string line;
    bool found = false;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        auto entry = core::Json::parse(line);
        if (entry && entry->find("file") && entry->find("file")->asString() == reportFileName) {
            update(*entry);
            line = entry->dump();
            found = true;
        }
        rewritten += line + '\n';
    }
    input.close();
    if (!found) {
        return false;
    }
    auto temporary = historyPath();
    temporary += L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << rewritten;
        if (!output) {
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, historyPath(), error);
    return !error;
}

std::vector<core::Json> HistoryStore::load() const {
    std::vector<core::Json> entries;
    std::ifstream stream(historyPath(), std::ios::binary);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        auto entry = core::Json::parse(line);
        if (!entry || !entry->isObject()) {
            continue;
        }
        const auto file = entry->find("file");
        std::error_code error;
        if (file == nullptr || !std::filesystem::exists(directory_ / core::toWide(file->asString()), error)) {
            continue;  // Report deleted by the user: drop it from the index.
        }
        entries.push_back(std::move(*entry));
    }
    return entries;
}

bool HistoryStore::writeIndex(std::string_view indexTemplate, std::string_view languageTag) const {
    core::Json payload = core::Json::object();
    payload["lang"] = languageTag;
    core::Json sessions = core::Json::array();
    for (auto& entry : load()) {
        sessions.push(std::move(entry));
    }
    payload["sessions"] = std::move(sessions);

    const auto html = renderTemplate(indexTemplate, payload);
    if (!html) {
        return false;
    }
    std::ofstream stream(indexPath(), std::ios::binary | std::ios::trunc);
    stream.write(html->data(), static_cast<std::streamsize>(html->size()));
    return static_cast<bool>(stream);
}

}  // namespace perfpulse::report
