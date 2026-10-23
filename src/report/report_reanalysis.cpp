#include "report/report_reanalysis.hpp"

#include <algorithm>

#include "analysis/bottleneck_analyzer.hpp"
#include "core/version.hpp"
#include "report/report_json.hpp"

namespace perfpulse::report {

namespace {

using core::Json;
using session::SensorSample;

constexpr std::string_view kScriptOpen = R"(<script id="perfpulse-data" type="application/json">)";
constexpr std::string_view kScriptClose = "</script>";

// Inverse of the channel table in report_json.cpp.
struct ChannelSetter {
    const char* key;
    float SensorSample::*field;
};

constexpr ChannelSetter kSetters[] = {
    {"gpu_usage", &SensorSample::gpuUsage},
    {"gpu_temperature", &SensorSample::gpuTemperature},
    {"gpu_hotspot", &SensorSample::gpuHotspot},
    {"gpu_core_clock", &SensorSample::gpuCoreClock},
    {"gpu_memory_clock", &SensorSample::gpuMemoryClock},
    {"gpu_power", &SensorSample::gpuPower},
    {"gpu_voltage", &SensorSample::gpuVoltage},
    {"gpu_fan", &SensorSample::gpuFan},
    {"gpu_fan_rpm", &SensorSample::gpuFanRpm},
    {"vram_used_gb", &SensorSample::vramUsedGb},
    {"vram_temperature", &SensorSample::vramTemperature},
    {"cpu_usage", &SensorSample::cpuUsage},
    {"cpu_busiest_core", &SensorSample::cpuBusiestCore},
    {"cpu_temperature", &SensorSample::cpuTemperature},
    {"cpu_clock_average", &SensorSample::cpuClockAverage},
    {"cpu_clock_max", &SensorSample::cpuClockMax},
    {"cpu_power", &SensorSample::cpuPower},
    {"game_cpu_usage", &SensorSample::gameCpuUsage},
    {"game_busiest_thread", &SensorSample::gameBusiestThread},
    {"game_working_set_gb", &SensorSample::gameWorkingSetGb},
    {"ram_used_gb", &SensorSample::ramUsedGb},
};

float valueAt(const Json* series, size_t index) {
    if (series == nullptr || index >= series->size()) {
        return session::kNoValue;
    }
    const auto& value = series->asArray()[index];
    return value.isNumber() ? static_cast<float>(value.asNumber()) : session::kNoValue;
}

// Totals are not exported, but used and percent are: total = used / percent.
void deriveTotal(float used, float percent, float& total) {
    if (session::hasValue(used) && session::hasValue(percent) && percent > 0.0f) {
        total = used / percent * 100.0f;
    }
}

}  // namespace

std::optional<Json> extractPayload(std::string_view html) {
    const auto open = html.find(kScriptOpen);
    if (open == std::string_view::npos) {
        return std::nullopt;
    }
    const auto start = open + kScriptOpen.size();
    const auto close = html.find(kScriptClose, start);
    if (close == std::string_view::npos) {
        return std::nullopt;
    }
    return Json::parse(html.substr(start, close - start));
}

std::optional<Json> reanalyzePayload(const Json& payload) {
    const Json* series = payload.find("series");
    const Json* time = series != nullptr ? series->find("t") : nullptr;
    const Json* fps = series != nullptr ? series->find("fps") : nullptr;
    if (time == nullptr || fps == nullptr || time->size() == 0) {
        return std::nullopt;
    }
    const size_t count = time->size();
    const Json* foreground = series->find("foreground");
    const Json* limiter = series->find("gpu_limiter");
    const Json* maxFrametime = series->find("ft_max");
    const Json* vramPercent = series->find("vram_usage_percent");
    const Json* ramPercent = series->find("ram_usage_percent");

    std::vector<SensorSample> samples(count);
    std::vector<analysis::SampleFrameStats> frames(count);
    for (size_t i = 0; i < count; ++i) {
        auto& s = samples[i];
        s.timeSec = time->asArray()[i].asNumber();
        s.gameInForeground = foreground == nullptr || i >= foreground->size() || foreground->asArray()[i].asInt(1) != 0;
        const Json* flags = limiter != nullptr && i < limiter->size() ? &limiter->asArray()[i] : nullptr;
        s.gpuLimiters = flags != nullptr && flags->isNumber() ? static_cast<uint32_t>(flags->asNumber())
                                                              : session::kLimiterUnknown;
        for (const auto& setter : kSetters) {
            s.*setter.field = valueAt(series->find(setter.key), i);
        }
        deriveTotal(s.vramUsedGb, valueAt(vramPercent, i), s.vramTotalGb);
        deriveTotal(s.ramUsedGb, valueAt(ramPercent, i), s.ramTotalGb);
        const float sampleFps = valueAt(fps, i);
        frames[i].fps = session::hasValue(sampleFps) ? sampleFps : 0.0f;
        const float worst = valueAt(maxFrametime, i);
        frames[i].maxFrametimeMs = session::hasValue(worst) ? worst : 0.0f;
    }

    const Json* info = payload.find("info");
    const Json* hardware = info != nullptr ? info->find("hardware") : nullptr;
    const int refreshHz = info != nullptr && info->find("monitor_hz") ? info->find("monitor_hz")->asInt() : 0;
    const Json* tjMax = hardware != nullptr ? hardware->find("cpu_tjmax") : nullptr;
    const float cpuTjMax =
        tjMax != nullptr && tjMax->isNumber() ? static_cast<float>(tjMax->asNumber()) : session::kNoValue;
    const auto summary = analysis::analyzeBottlenecks(samples, frames, refreshHz, cpuTjMax);

    Json result = payload;
    result["bottleneck"] = buildBottleneckJson(summary);
    Json timeline = Json::array();
    for (const auto b : summary.timeline) {
        timeline.push(static_cast<int>(b));
    }
    result["series"]["bottleneck"] = std::move(timeline);

    // Each stutter takes the classification of the sample covering its time.
    if (Json* stutters = &result["stutters"]; stutters->isArray()) {
        for (auto& stutter : stutters->asArray()) {
            const double t = stutter.find("t") ? stutter.find("t")->asNumber() : 0.0;
            const auto it = std::lower_bound(samples.begin(), samples.end(), t,
                                             [](const SensorSample& s, double v) { return s.timeSec < v; });
            const auto index = std::min(static_cast<size_t>(it - samples.begin()), summary.timeline.size() - 1);
            stutter["bottleneck"] = analysis::bottleneckKey(summary.timeline[index]);
        }
    }
    result["reanalyzed_by"] = std::string(core::kAppName) + " " + core::kVersion;
    return result;
}

}  // namespace perfpulse::report
