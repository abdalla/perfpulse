#pragma once

#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/metrics.hpp"

namespace perfpulse::core {

enum class FinishAction { Notify, OpenReport, Silent };
enum class Language { Auto, PortugueseBrazil, English };

struct Config {
    bool monitoringEnabled = true;
    // GDI device name of the monitor to watch (e.g. "\\.\DISPLAY1"). Empty means any monitor.
    std::string monitorDevice;
    bool requireFullscreen = true;
    int detectionDelaySec = 20;
    int sampleIntervalMs = 1000;
    int minSessionSec = 30;
    // Empty means <Documents>\PerfPulse\Reports.
    std::string reportDirectory;
    FinishAction finishAction = FinishAction::Notify;
    Language language = Language::Auto;
    bool startWithWindows = false;
    // Lower-case executable names, e.g. "cyberpunk2077.exe".
    std::vector<std::string> alwaysMonitor;
    std::vector<std::string> neverMonitor;
    MetricSet metrics = MetricSet::defaults();

    static Config defaults();
    static Config fromJson(const Json& json);
    Json toJson() const;

    bool isAlwaysMonitored(std::string_view exeName) const;
    bool isNeverMonitored(std::string_view exeName) const;
};

// Executables that render with the GPU but are never games.
const std::vector<std::string>& builtInExclusions();

// Sample interval choices offered in the UI, in milliseconds.
inline constexpr int kSampleIntervalChoicesMs[] = {250, 500, 1000, 2000, 5000};

}  // namespace perfpulse::core
