#include "app/diagnostics.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "capture/frame_capture.hpp"
#include "core/i18n.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"
#include "detection/monitor_info.hpp"
#include "detection/process_info.hpp"
#include "app/resource_ids.h"
#include "report/demo_session.hpp"
#include "report/history_store.hpp"
#include "report/html_report_writer.hpp"
#include "report/report_generator.hpp"
#include "report/report_reanalysis.hpp"
#include "report/resource_loader.hpp"
#include "sensors/sensor_hub.hpp"
#include "sensors/system_info.hpp"

namespace perfpulse::app {

namespace {

class Output {
public:
    explicit Output(const std::filesystem::path& file) {
        if (!file.empty()) {
            _wfopen_s(&file_, file.c_str(), L"w");
            return;
        }
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }
        SetConsoleOutputCP(CP_UTF8);
        _wfopen_s(&file_, L"CONOUT$", L"w");
    }
    ~Output() {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }
    void line(const std::string& text = {}) {
        if (file_ != nullptr) {
            std::fprintf(file_, "%s\n", text.c_str());
            std::fflush(file_);
        }
    }

private:
    FILE* file_ = nullptr;
};

std::string value(float v, const char* unit, int decimals = 1) {
    if (!session::hasValue(v)) {
        return "N/A";
    }
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.*f %s", decimals, static_cast<double>(v), unit);
    return buffer;
}

}  // namespace

int runDiagnostics(const std::filesystem::path& outputFile) {
    Output out(outputFile);
    out.line("PerfPulse diagnostics");
    out.line("=====================");
    out.line(std::string("Elevated: ") + (sensors::isProcessElevated() ? "yes" : "NO (ETW and PawnIO need admin)"));

    out.line();
    out.line("Monitors:");
    const auto monitors = detection::enumerateMonitors();
    for (size_t i = 0; i < monitors.size(); ++i) {
        out.line("  " + monitors[i].displayLabel(i) + "  [" + monitors[i].deviceName + "]");
    }

    out.line();
    out.line("Sensors:");
    {
        sensors::SensorHub hub;
        session::HardwareInfo hw;
        std::string gpuSource;
        std::string cpuSource;
        hub.describe(hw, gpuSource, cpuSource);
        out.line("  CPU: " + hw.cpuName + " (" + std::to_string(hw.cpuPerformanceCores) + "P + " +
                 std::to_string(hw.cpuEfficiencyCores) + "E, " + std::to_string(hw.cpuLogicalProcessors) +
                 " threads, TjMax " + value(hw.cpuTjMax, "C", 0) + ")");
        out.line("  GPU: " + hw.gpuName + " driver " + hw.gpuDriver + " limit " + value(hw.gpuPowerLimitW, "W", 0));
        out.line("  RAM: " + value(hw.ramTotalGb, "GB"));
        out.line("  OS : " + hw.osVersion);
        out.line("  GPU source: " + gpuSource);
        out.line("  CPU temperature/power source: " + cpuSource);

        const auto request = sensors::makeSensorRequest(core::MetricSet::everything(), GetCurrentProcessId());
        for (int i = 0; i < 3; ++i) {
            Sleep(1000);
            session::SensorSample s;
            hub.read(s, request);
            out.line("  sample " + std::to_string(i + 1) + ": GPU " + value(s.gpuUsage, "%") + " " +
                     value(s.gpuTemperature, "C") + " hotspot " + value(s.gpuHotspot, "C") + " " +
                     value(s.gpuPower, "W") + " core " + value(s.gpuCoreClock, "MHz", 0) + " mem " +
                     value(s.gpuMemoryClock, "MHz", 0) + " " + value(s.gpuVoltage, "V", 3) + " fan " +
                     value(s.gpuFan, "%", 0) + " limiter " + std::to_string(s.gpuLimiters));
            out.line("            VRAM " + value(s.vramUsedGb, "GB", 2) + "/" + value(s.vramTotalGb, "GB", 1) +
                     " junction " + value(s.vramTemperature, "C") + " | CPU " + value(s.cpuUsage, "%") +
                     " busiest " + value(s.cpuBusiestCore, "%") + " " + value(s.cpuTemperature, "C") + " " +
                     value(s.cpuPower, "W") + " clock " + value(s.cpuClockAverage, "MHz", 0) + "/" +
                     value(s.cpuClockMax, "MHz", 0) + " | RAM " + value(s.ramUsedGb, "GB", 2));
        }
    }

    out.line();
    out.line("Frame capture (ETW), 10 seconds:");
    capture::FrameCapture capture;
    if (!capture.start()) {
        out.line(capture.status() == capture::FrameCapture::Status::AccessDenied ? "  access denied: run elevated"
                                                                               : "  failed to start");
        return 1;
    }
    for (int round = 0; round < 5; ++round) {
        Sleep(2000);
        auto activity = capture.takeActivity();
        std::sort(activity.begin(), activity.end(),
                  [](const auto& a, const auto& b) { return a.presents > b.presents; });
        out.line("  t+" + std::to_string((round + 1) * 2) + "s:");
        for (size_t i = 0; i < std::min<size_t>(activity.size(), 6); ++i) {
            const auto process = detection::queryProcess(activity[i].processId);
            char rate[32];
            std::snprintf(rate, sizeof(rate), "%6.1f presents/s", activity[i].presents / 2.0);
            out.line(std::string("    ") + rate + "  pid " + std::to_string(activity[i].processId) + "  " +
                     (process.exeName.empty() ? "?" : process.exeName));
        }
    }
    out.line("  events by (source, id):");
    for (const auto& [key, count] : capture.eventCounts()) {
        out.line("    " +
                 std::string(capture::presentSourceName(static_cast<capture::PresentSource>(key.first))) +
                 " #" + std::to_string(key.second) + ": " + std::to_string(count));
    }
    out.line("  events lost: " + std::to_string(capture.eventsLost()));
    capture.stop();
    return 0;
}

int runReanalyze(const std::filesystem::path& report, const std::filesystem::path& destination,
                 const std::filesystem::path& outputFile) {
    Output out(outputFile);
    const auto html = core::readTextFile(report);
    const auto payload = html ? report::extractPayload(*html) : std::nullopt;
    if (!payload) {
        out.line("Not a PerfPulse report: " + core::toUtf8(report.wstring()));
        return 1;
    }
    const auto updated = report::reanalyzePayload(*payload);
    if (!updated) {
        out.line("The report has no per-second series to reanalyze");
        return 1;
    }
    const auto rendered = report::renderTemplate(report::loadTextResource(IDR_REPORT_TEMPLATE), *updated);
    const auto target = destination.empty() ? report : destination;
    if (!rendered || !core::writeTextFileAtomic(target, *rendered)) {
        out.line("Could not write " + core::toUtf8(target.wstring()));
        return 1;
    }

    const auto& bottleneck = *updated->find("bottleneck");
    const auto language = updated->find("lang") ? updated->find("lang")->asString("pt-BR") : "pt-BR";
    const report::HistoryStore history(target.parent_path());
    const bool historyUpdated =
        history.updateEntry(core::toUtf8(target.filename().wstring()), [&](core::Json& entry) {
            entry["offender"] = *bottleneck.find("offender");
            entry["offender_share"] = *bottleneck.find("offender_share");
        }) &&
        history.writeIndex(report::loadTextResource(IDR_INDEX_TEMPLATE), language);

    const auto before = payload->find("bottleneck");
    out.line("Report: " + core::toUtf8(target.wstring()));
    out.line("Bottleneck: " + (before ? before->find("offender")->asString() : "?") + " -> " +
             bottleneck.find("offender")->asString() + " (" +
             std::to_string(static_cast<int>(bottleneck.find("offender_share")->asNumber() * 100.0 + 0.5)) + "%)");
    out.line(std::string("History: ") + (historyUpdated ? "updated" : "entry not found, left unchanged"));
    return 0;
}

int runDemoReport(const std::filesystem::path& outputFile) {
    const auto config = core::loadConfig();
    core::setLanguage(config.language);
    const auto data = report::makeDemoSession(core::MetricSet::everything());
    const auto report = report::generateReport(data, config);
    Output out(outputFile);
    out.line(report ? core::toUtf8(report->path.wstring()) : "failed");
    return report ? 0 : 1;
}

}  // namespace perfpulse::app
