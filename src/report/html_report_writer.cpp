#include "report/html_report_writer.hpp"

#include <cstdio>
#include <ctime>

#include "core/strings.hpp"

namespace perfpulse::report {

namespace {

bool toLocalTime(int64_t unixMs, std::tm& out) {
    const std::time_t seconds = static_cast<std::time_t>(unixMs / 1000);
    return localtime_s(&out, &seconds) == 0;
}

}  // namespace

std::optional<std::string> renderTemplate(std::string_view templateHtml, const core::Json& payload) {
    const auto position = templateHtml.find(kDataPlaceholder);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }
    std::string html;
    html.reserve(templateHtml.size() + 256 * 1024);
    html.append(templateHtml.substr(0, position));
    payload.dumpTo(html);
    html.append(templateHtml.substr(position + kDataPlaceholder.size()));
    return html;
}

std::string reportFileName(const session::SessionInfo& info) {
    std::tm local{};
    char stamp[32] = "unknown";
    if (toLocalTime(info.startUnixMs, local)) {
        std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d_%02d%02d", local.tm_year + 1900, local.tm_mon + 1,
                      local.tm_mday, local.tm_hour, local.tm_min);
    }
    const auto& name = info.gameName.empty() ? info.exeName : info.gameName;
    return std::string("PerfPulse_") + stamp + "_" + core::sanitizeFileName(name) + ".html";
}

std::string formatLocalDateTime(int64_t unixMs) {
    std::tm local{};
    if (!toLocalTime(unixMs, local)) {
        return {};
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", local.tm_year + 1900, local.tm_mon + 1,
                  local.tm_mday, local.tm_hour, local.tm_min);
    return text;
}

}  // namespace perfpulse::report
