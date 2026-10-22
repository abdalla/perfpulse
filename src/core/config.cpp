#include "core/config.hpp"

#include <algorithm>

#include "core/strings.hpp"

namespace perfpulse::core {

namespace {

std::string_view finishActionKey(FinishAction action) {
    switch (action) {
        case FinishAction::OpenReport: return "open";
        case FinishAction::Silent: return "silent";
        case FinishAction::Notify: break;
    }
    return "notify";
}

FinishAction finishActionFromKey(std::string_view key) {
    if (key == "open") {
        return FinishAction::OpenReport;
    }
    if (key == "silent") {
        return FinishAction::Silent;
    }
    return FinishAction::Notify;
}

std::string_view languageKey(Language language) {
    switch (language) {
        case Language::PortugueseBrazil: return "pt-BR";
        case Language::English: return "en";
        case Language::Auto: break;
    }
    return "auto";
}

Language languageFromKey(std::string_view key) {
    if (key == "pt-BR") {
        return Language::PortugueseBrazil;
    }
    if (key == "en") {
        return Language::English;
    }
    return Language::Auto;
}

std::vector<std::string> readExeList(const Json* json) {
    std::vector<std::string> result;
    if (json == nullptr) {
        return result;
    }
    for (const auto& item : json->asArray()) {
        auto name = toLower(trim(item.asString()));
        if (!name.empty()) {
            result.push_back(std::move(name));
        }
    }
    return result;
}

Json writeExeList(const std::vector<std::string>& list) {
    Json array = Json::array();
    for (const auto& name : list) {
        array.push(name);
    }
    return array;
}

bool containsExe(const std::vector<std::string>& list, std::string_view exeName) {
    const auto lower = toLower(exeName);
    return std::find(list.begin(), list.end(), lower) != list.end();
}

}  // namespace

Config Config::defaults() {
    return Config{};
}

Config Config::fromJson(const Json& json) {
    Config config;
    if (const auto* v = json.find("monitoring_enabled")) {
        config.monitoringEnabled = v->asBool(config.monitoringEnabled);
    }
    if (const auto* v = json.find("monitor_device")) {
        config.monitorDevice = v->asString();
    }
    if (const auto* v = json.find("require_fullscreen")) {
        config.requireFullscreen = v->asBool(config.requireFullscreen);
    }
    if (const auto* v = json.find("detection_delay_sec")) {
        config.detectionDelaySec = std::clamp(v->asInt(config.detectionDelaySec), 3, 300);
    }
    if (const auto* v = json.find("sample_interval_ms")) {
        config.sampleIntervalMs = std::clamp(v->asInt(config.sampleIntervalMs), 250, 5000);
    }
    if (const auto* v = json.find("min_session_sec")) {
        config.minSessionSec = std::clamp(v->asInt(config.minSessionSec), 5, 3600);
    }
    if (const auto* v = json.find("report_directory")) {
        config.reportDirectory = v->asString();
    }
    if (const auto* v = json.find("finish_action")) {
        config.finishAction = finishActionFromKey(v->asString());
    }
    if (const auto* v = json.find("language")) {
        config.language = languageFromKey(v->asString());
    }
    if (const auto* v = json.find("start_with_windows")) {
        config.startWithWindows = v->asBool(config.startWithWindows);
    }
    config.alwaysMonitor = readExeList(json.find("always_monitor"));
    config.neverMonitor = readExeList(json.find("never_monitor"));
    if (const auto* metrics = json.find("metrics"); metrics != nullptr && metrics->isObject()) {
        config.metrics.clear();
        for (const auto& [key, enabled] : metrics->asObject()) {
            if (const auto metric = metricFromKey(key)) {
                config.metrics.set(*metric, enabled.asBool());
            }
        }
    }
    return config;
}

Json Config::toJson() const {
    Json json = Json::object();
    json["monitoring_enabled"] = monitoringEnabled;
    json["monitor_device"] = monitorDevice;
    json["require_fullscreen"] = requireFullscreen;
    json["detection_delay_sec"] = detectionDelaySec;
    json["sample_interval_ms"] = sampleIntervalMs;
    json["min_session_sec"] = minSessionSec;
    json["report_directory"] = reportDirectory;
    json["finish_action"] = finishActionKey(finishAction);
    json["language"] = languageKey(language);
    json["start_with_windows"] = startWithWindows;
    json["always_monitor"] = writeExeList(alwaysMonitor);
    json["never_monitor"] = writeExeList(neverMonitor);
    Json metricsJson = Json::object();
    for (const auto& info : allMetrics()) {
        metricsJson[info.key] = metrics.has(info.metric);
    }
    json["metrics"] = std::move(metricsJson);
    return json;
}

bool Config::isAlwaysMonitored(std::string_view exeName) const {
    return containsExe(alwaysMonitor, exeName);
}

bool Config::isNeverMonitored(std::string_view exeName) const {
    return containsExe(neverMonitor, exeName) || containsExe(builtInExclusions(), exeName);
}

const std::vector<std::string>& builtInExclusions() {
    static const std::vector<std::string> kExclusions = {
        // Shell and system
        "explorer.exe", "applicationframehost.exe", "searchhost.exe", "startmenuexperiencehost.exe",
        "shellexperiencehost.exe", "textinputhost.exe", "lockapp.exe", "dwm.exe", "taskmgr.exe",
        "systemsettings.exe", "windowsterminal.exe", "conhost.exe", "cmd.exe", "powershell.exe",
        // Browsers and web runtimes
        "chrome.exe", "msedge.exe", "firefox.exe", "opera.exe", "opera_gx.exe", "brave.exe", "vivaldi.exe",
        "msedgewebview2.exe", "arc.exe",
        // Communication and streaming
        "discord.exe", "teams.exe", "ms-teams.exe", "slack.exe", "whatsapp.exe", "telegram.exe", "zoom.exe",
        "obs64.exe", "obs32.exe", "streamlabs obs.exe", "spotify.exe", "vlc.exe", "mpc-hc64.exe", "mpv.exe",
        "potplayermini64.exe",
        // Launchers and overlays
        "steam.exe", "steamwebhelper.exe", "epicgameslauncher.exe", "battle.net.exe", "eadesktop.exe",
        "galaxyclient.exe", "ubisoftconnect.exe", "upc.exe", "riotclientux.exe", "nvidia app.exe",
        "nvcontainer.exe", "radeonsoftware.exe", "msiafterburner.exe", "rtss.exe",
        // Productivity
        "code.exe", "devenv.exe", "claude.exe", "notepad.exe", "winword.exe", "excel.exe", "powerpnt.exe",
        "outlook.exe", "olk.exe", "photoshop.exe", "blender.exe", "perfpulse.exe",
    };
    return kExclusions;
}

}  // namespace perfpulse::core
