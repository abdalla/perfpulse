#include "report/report_generator.hpp"

#include "analysis/session_analysis.hpp"
#include "app/resource_ids.h"
#include "core/i18n.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"
#include "report/history_store.hpp"
#include "report/html_report_writer.hpp"
#include "report/report_json.hpp"
#include "report/resource_loader.hpp"

namespace perfpulse::report {

namespace {

// Avoids overwriting a report when two sessions start in the same minute.
std::filesystem::path uniquePath(const std::filesystem::path& directory, const std::string& fileName) {
    auto path = directory / core::toWide(fileName);
    std::error_code error;
    for (int suffix = 2; std::filesystem::exists(path, error); ++suffix) {
        auto stem = std::filesystem::path(core::toWide(fileName)).stem().wstring();
        path = directory / (stem + L"_" + std::to_wstring(suffix) + L".html");
    }
    return path;
}

}  // namespace

std::filesystem::path historyIndexPath(const core::Config& config) {
    return HistoryStore(core::reportDirectory(config)).indexPath();
}

std::optional<GeneratedReport> generateReport(const session::SessionData& data, const core::Config& config) {
    const auto analysis = analysis::analyzeSession(data);
    const auto language = core::languageTag(config.language);
    const auto payload = buildReportPayload(data, analysis, language);

    const auto html = renderTemplate(loadTextResource(IDR_REPORT_TEMPLATE), payload);
    if (!html) {
        core::log::error("Report template missing from resources");
        return std::nullopt;
    }
    const auto directory = core::reportDirectory(config);
    const auto path = uniquePath(directory, reportFileName(data.info));
    if (!core::writeTextFileAtomic(path, *html)) {
        return std::nullopt;
    }

    const HistoryStore history(directory);
    if (!history.append(buildHistoryEntry(data, analysis, core::toUtf8(path.filename().wstring()))) ||
        !history.writeIndex(loadTextResource(IDR_INDEX_TEMPLATE), language)) {
        core::log::warn("Could not update the session history");
    }

    GeneratedReport report;
    report.path = path;
    report.gameName = data.info.gameName;
    report.averageFps = analysis.frames.averageFps;
    report.grade = analysis.smoothness.grade;
    report.durationSec = data.durationSec;
    core::log::info("Report written: " + core::toUtf8(path.wstring()));
    return report;
}

}  // namespace perfpulse::report
