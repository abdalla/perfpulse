#include "report/report_json.hpp"

#include <cmath>

#include "core/version.hpp"

namespace perfpulse::report {

namespace {

using analysis::Bottleneck;
using analysis::ChannelSummary;
using analysis::ComponentEnergy;
using core::Json;
using session::SensorSample;

double roundTo(double value, int decimals) {
    if (!std::isfinite(value)) {
        return value;
    }
    const double factor = std::pow(10.0, decimals);
    return std::round(value * factor) / factor;
}

Json number(double value, int decimals = 2) {
    if (!std::isfinite(value)) {
        return Json(nullptr);
    }
    return Json(roundTo(value, decimals));
}

Json channelJson(const ChannelSummary& summary) {
    if (!summary.available) {
        return Json(nullptr);
    }
    Json json = Json::object();
    json["avg"] = number(summary.average);
    json["median"] = number(summary.median);
    json["min"] = number(summary.min);
    json["max"] = number(summary.max);
    json["p95"] = number(summary.p95);
    return json;
}

Json energyJson(const ComponentEnergy& energy) {
    Json json = Json::object();
    json["measured"] = energy.measured;
    json["wh"] = number(energy.wattHours, 3);
    json["avg_w"] = number(energy.averageWatts, 1);
    json["peak_w"] = number(energy.peakWatts, 1);
    return json;
}

struct Channel {
    const char* key;
    float (*get)(const SensorSample&);
    int decimals;
};

// Every sensor channel exported to the report. Keys match core::Metric keys
// where a metric exists, so the template can filter by the user's selection.
constexpr Channel kChannels[] = {
    {"gpu_usage", [](const SensorSample& s) { return s.gpuUsage; }, 1},
    {"gpu_temperature", [](const SensorSample& s) { return s.gpuTemperature; }, 1},
    {"gpu_hotspot", [](const SensorSample& s) { return s.gpuHotspot; }, 1},
    {"gpu_core_clock", [](const SensorSample& s) { return s.gpuCoreClock; }, 0},
    {"gpu_memory_clock", [](const SensorSample& s) { return s.gpuMemoryClock; }, 0},
    {"gpu_power", [](const SensorSample& s) { return s.gpuPower; }, 1},
    {"gpu_voltage", [](const SensorSample& s) { return s.gpuVoltage; }, 3},
    {"gpu_fan", [](const SensorSample& s) { return s.gpuFan; }, 0},
    {"gpu_fan_rpm", [](const SensorSample& s) { return s.gpuFanRpm; }, 0},
    {"vram_used_gb", [](const SensorSample& s) { return s.vramUsedGb; }, 2},
    {"vram_usage_percent",
     [](const SensorSample& s) {
         return session::hasValue(s.vramUsedGb) && s.vramTotalGb > 0.0f ? s.vramUsedGb / s.vramTotalGb * 100.0f
                                                                         : session::kNoValue;
     },
     1},
    {"vram_temperature", [](const SensorSample& s) { return s.vramTemperature; }, 1},
    {"cpu_usage", [](const SensorSample& s) { return s.cpuUsage; }, 1},
    {"cpu_busiest_core", [](const SensorSample& s) { return s.cpuBusiestCore; }, 1},
    {"cpu_temperature", [](const SensorSample& s) { return s.cpuTemperature; }, 1},
    {"cpu_clock_average", [](const SensorSample& s) { return s.cpuClockAverage; }, 0},
    {"cpu_clock_max", [](const SensorSample& s) { return s.cpuClockMax; }, 0},
    {"cpu_power", [](const SensorSample& s) { return s.cpuPower; }, 1},
    {"game_cpu_usage", [](const SensorSample& s) { return s.gameCpuUsage; }, 1},
    {"game_busiest_thread", [](const SensorSample& s) { return s.gameBusiestThread; }, 1},
    {"game_working_set_gb", [](const SensorSample& s) { return s.gameWorkingSetGb; }, 2},
    {"ram_used_gb", [](const SensorSample& s) { return s.ramUsedGb; }, 2},
    {"ram_usage_percent",
     [](const SensorSample& s) {
         return session::hasValue(s.ramUsedGb) && s.ramTotalGb > 0.0f ? s.ramUsedGb / s.ramTotalGb * 100.0f
                                                                       : session::kNoValue;
     },
     1},
};

Json infoJson(const session::SessionData& data, const analysis::SessionAnalysis& analysis) {
    const auto& info = data.info;
    Json json = Json::object();
    json["game_name"] = info.gameName;
    json["exe_name"] = info.exeName;
    json["exe_path"] = info.exePath;
    json["window_title"] = info.windowTitle;
    json["present_api"] = info.presentApi;
    json["sync_interval"] = info.dominantSyncInterval;
    json["monitor_name"] = info.monitorName;
    json["monitor_hz"] = info.monitorRefreshHz;
    json["monitor_width"] = info.monitorWidth;
    json["monitor_height"] = info.monitorHeight;
    json["start_ms"] = info.startUnixMs;
    json["end_ms"] = info.endUnixMs;
    json["duration_sec"] = number(data.durationSec, 1);
    json["foreground_sec"] = number(analysis.foregroundSec, 1);
    json["background_sec"] = number(analysis.backgroundSec, 1);
    json["sample_interval_sec"] = number(data.sampleIntervalSec, 3);
    json["gpu_source"] = info.gpuSource;
    json["cpu_sensor_source"] = info.cpuSensorSource;
    json["etw_events_lost"] = info.etwEventsLost;

    const auto& hw = info.hardware;
    Json hardware = Json::object();
    hardware["cpu_name"] = hw.cpuName;
    hardware["cpu_p_cores"] = hw.cpuPerformanceCores;
    hardware["cpu_e_cores"] = hw.cpuEfficiencyCores;
    hardware["cpu_threads"] = hw.cpuLogicalProcessors;
    hardware["cpu_tjmax"] = number(hw.cpuTjMax, 0);
    hardware["gpu_name"] = hw.gpuName;
    hardware["gpu_driver"] = hw.gpuDriver;
    hardware["gpu_power_limit_w"] = number(hw.gpuPowerLimitW, 0);
    hardware["ram_total_gb"] = number(hw.ramTotalGb, 1);
    hardware["os"] = hw.osVersion;
    json["hardware"] = std::move(hardware);
    return json;
}

Json framesJson(const analysis::FrameStatistics& f) {
    Json json = Json::object();
    json["count"] = f.frameCount;
    json["active_sec"] = number(f.activeSec, 1);
    json["avg_fps"] = number(f.averageFps, 1);
    json["median_fps"] = number(f.medianFps, 1);
    json["low1_fps"] = number(f.onePercentLowFps, 1);
    json["low01_fps"] = number(f.pointOnePercentLowFps, 1);
    json["low1_avg_fps"] = number(f.onePercentLowAverageFps, 1);
    json["ft_avg"] = number(f.averageFrametimeMs, 2);
    json["ft_median"] = number(f.medianFrametimeMs, 2);
    json["ft_p95"] = number(f.p95FrametimeMs, 2);
    json["ft_p99"] = number(f.p99FrametimeMs, 2);
    json["ft_p999"] = number(f.p999FrametimeMs, 2);
    json["ft_max"] = number(f.maxFrametimeMs, 2);
    json["ft_stddev"] = number(f.stdDevFrametimeMs, 2);
    json["jitter"] = number(f.relativeJitter, 4);
    Json histogram = Json::array();
    for (const auto& bin : f.histogram) {
        histogram.push(Json(Json::Array{number(bin.fromMs), number(bin.toMs), Json(bin.count)}));
    }
    json["histogram"] = std::move(histogram);
    Json percentiles = Json::array();
    for (const auto& point : f.percentileCurve) {
        percentiles.push(Json(Json::Array{number(point.percentile, 2), number(point.ms, 2)}));
    }
    json["percentiles"] = std::move(percentiles);
    return json;
}

Json bottleneckJson(const analysis::BottleneckSummary& b) {
    Json json = Json::object();
    json["offender"] = analysis::bottleneckKey(b.offender);
    json["offender_share"] = number(b.offenderShare, 4);
    Json shares = Json::object();
    for (size_t i = 0; i < analysis::kBottleneckCount; ++i) {
        shares[analysis::bottleneckKey(static_cast<Bottleneck>(i))] = number(b.shareOfActiveTime[i], 4);
    }
    json["shares"] = std::move(shares);
    json["cap_fps"] = number(b.detectedCapFps, 0);
    json["uncapped_estimate_fps"] = number(b.uncappedEstimateFps, 0);
    json["cpu_bound_thread"] = number(b.cpuBoundBusiestThread, 1);
    json["cpu_bound_gpu"] = number(b.cpuBoundGpuUsage, 1);
    json["gpu_power_limited_share"] = number(b.gpuPowerLimitedShare, 4);
    return json;
}

Json seriesJson(const session::SessionData& data, const analysis::SessionAnalysis& analysis) {
    Json series = Json::object();
    Json time = Json::array();
    Json fps = Json::array();
    Json maxFrametime = Json::array();
    Json foreground = Json::array();
    Json limiter = Json::array();
    Json bottleneck = Json::array();
    for (size_t i = 0; i < data.samples.size(); ++i) {
        const auto& s = data.samples[i];
        time.push(number(s.timeSec, 2));
        foreground.push(s.gameInForeground ? 1 : 0);
        limiter.push(s.gpuLimiters == session::kLimiterUnknown ? Json(nullptr) : Json(s.gpuLimiters));
        if (i < analysis.perSample.size()) {
            fps.push(number(analysis.perSample[i].fps, 1));
            maxFrametime.push(number(analysis.perSample[i].maxFrametimeMs, 2));
        }
        if (i < analysis.bottleneck.timeline.size()) {
            bottleneck.push(static_cast<int>(analysis.bottleneck.timeline[i]));
        }
    }
    series["t"] = std::move(time);
    series["fps"] = std::move(fps);
    series["ft_max"] = std::move(maxFrametime);
    series["foreground"] = std::move(foreground);
    series["gpu_limiter"] = std::move(limiter);
    series["bottleneck"] = std::move(bottleneck);
    for (const auto& channel : kChannels) {
        Json values = Json::array();
        bool any = false;
        for (const auto& s : data.samples) {
            const float value = channel.get(s);
            any = any || session::hasValue(value);
            values.push(number(value, channel.decimals));
        }
        series[channel.key] = any ? std::move(values) : Json(nullptr);
    }
    return series;
}

}  // namespace

Json buildReportPayload(const session::SessionData& data, const analysis::SessionAnalysis& analysis,
                        std::string_view languageTag) {
    Json payload = Json::object();
    payload["app"] = core::kAppName;
    payload["version"] = core::kVersion;
    payload["lang"] = languageTag;

    Json metrics = Json::object();
    for (const auto& info : core::allMetrics()) {
        metrics[info.key] = data.metrics.has(info.metric);
    }
    payload["metrics"] = std::move(metrics);
    payload["info"] = infoJson(data, analysis);
    payload["frames"] = framesJson(analysis.frames);

    Json score = Json::object();
    score["score"] = analysis.smoothness.score;
    score["grade"] = analysis.smoothness.grade;
    score["consistency"] = number(analysis.smoothness.consistency, 3);
    score["stutter_free"] = number(analysis.smoothness.stutterFree, 3);
    score["pacing"] = number(analysis.smoothness.pacing, 3);
    payload["score"] = std::move(score);

    payload["bottleneck"] = bottleneckJson(analysis.bottleneck);

    Json energy = Json::object();
    energy["gpu"] = energyJson(analysis.energy.gpu);
    energy["cpu"] = energyJson(analysis.energy.cpu);
    energy["total_wh"] = number(analysis.energy.totalWattHours, 3);
    energy["avg_w"] = number(analysis.energy.averageWatts, 1);
    energy["fps_per_watt"] = number(analysis.energy.fpsPerWatt, 3);
    energy["mj_per_frame"] = number(analysis.energy.millijoulesPerFrame, 1);
    payload["energy"] = std::move(energy);

    Json channels = Json::object();
    for (const auto& channel : kChannels) {
        channels[channel.key] = channelJson(analysis::summarizeChannel(data.samples, channel.get));
    }
    payload["channels"] = std::move(channels);
    payload["series"] = seriesJson(data, analysis);

    Json envelope = Json::object();
    Json t = Json::array();
    Json lo = Json::array();
    Json avg = Json::array();
    Json hi = Json::array();
    for (const auto& bucket : analysis.frametimeEnvelope) {
        t.push(number(bucket.timeSec, 2));
        lo.push(number(bucket.minMs, 2));
        avg.push(number(bucket.avgMs, 2));
        hi.push(number(bucket.maxMs, 2));
    }
    envelope["t"] = std::move(t);
    envelope["min"] = std::move(lo);
    envelope["avg"] = std::move(avg);
    envelope["max"] = std::move(hi);
    payload["envelope"] = std::move(envelope);

    Json stutters = Json::array();
    for (const auto& stutter : analysis.stutters) {
        Json item = Json::object();
        item["t"] = number(stutter.event.timeSec, 3);
        item["ms"] = number(stutter.event.frametimeMs, 1);
        item["median_ms"] = number(stutter.event.localMedianMs, 1);
        item["frames"] = stutter.event.framesInvolved;
        item["bottleneck"] = analysis::bottleneckKey(stutter.bottleneck);
        item["gpu_usage"] = number(stutter.sample.gpuUsage, 0);
        item["cpu_usage"] = number(stutter.sample.cpuUsage, 0);
        item["thread"] = number(stutter.sample.gameBusiestThread, 0);
        item["vram_gb"] = number(stutter.sample.vramUsedGb, 2);
        item["ram_gb"] = number(stutter.sample.ramUsedGb, 2);
        stutters.push(std::move(item));
    }
    payload["stutters"] = std::move(stutters);
    return payload;
}

Json buildHistoryEntry(const session::SessionData& data, const analysis::SessionAnalysis& analysis,
                       std::string_view reportFileName) {
    Json entry = Json::object();
    entry["file"] = reportFileName;
    entry["game"] = data.info.gameName;
    entry["exe"] = data.info.exeName;
    entry["start_ms"] = data.info.startUnixMs;
    entry["duration_sec"] = number(data.durationSec, 0);
    entry["avg_fps"] = number(analysis.frames.averageFps, 1);
    entry["median_fps"] = number(analysis.frames.medianFps, 1);
    entry["low1_fps"] = number(analysis.frames.onePercentLowFps, 1);
    entry["low01_fps"] = number(analysis.frames.pointOnePercentLowFps, 1);
    entry["score"] = analysis.smoothness.score;
    entry["grade"] = analysis.smoothness.grade;
    entry["stutters"] = analysis.stutters.size();
    entry["offender"] = analysis::bottleneckKey(analysis.bottleneck.offender);
    entry["offender_share"] = number(analysis.bottleneck.offenderShare, 3);
    entry["total_wh"] = number(analysis.energy.totalWattHours, 2);
    entry["fps_per_watt"] = number(analysis.energy.fpsPerWatt, 3);
    entry["gpu_name"] = data.info.hardware.gpuName;
    entry["gpu_driver"] = data.info.hardware.gpuDriver;
    entry["cpu_name"] = data.info.hardware.cpuName;
    entry["resolution"] = std::to_string(data.info.monitorWidth) + "x" + std::to_string(data.info.monitorHeight);
    return entry;
}

}  // namespace perfpulse::report
