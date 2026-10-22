#include "core/metrics.hpp"

namespace perfpulse::core {

namespace {

constexpr std::array<MetricInfo, kMetricCount> kMetrics = {{
    {Metric::Fps, MetricGroup::Fps, "fps"},
    {Metric::Frametime, MetricGroup::Fps, "frametime"},
    {Metric::OnePercentLow, MetricGroup::Fps, "one_percent_low"},
    {Metric::FrametimeGraph, MetricGroup::Fps, "frametime_graph"},
    {Metric::GpuUsage, MetricGroup::Gpu, "gpu_usage"},
    {Metric::GpuTemperature, MetricGroup::Gpu, "gpu_temperature"},
    {Metric::GpuHotspot, MetricGroup::Gpu, "gpu_hotspot"},
    {Metric::GpuCoreClock, MetricGroup::Gpu, "gpu_core_clock"},
    {Metric::GpuMemoryClock, MetricGroup::Gpu, "gpu_memory_clock"},
    {Metric::GpuPower, MetricGroup::Gpu, "gpu_power"},
    {Metric::GpuVoltage, MetricGroup::Gpu, "gpu_voltage"},
    {Metric::GpuFan, MetricGroup::Gpu, "gpu_fan"},
    {Metric::GpuLimiter, MetricGroup::Gpu, "gpu_limiter"},
    {Metric::VramUsedGb, MetricGroup::Vram, "vram_used_gb"},
    {Metric::VramUsagePercent, MetricGroup::Vram, "vram_usage_percent"},
    {Metric::VramTemperature, MetricGroup::Vram, "vram_temperature"},
    {Metric::CpuUsage, MetricGroup::Cpu, "cpu_usage"},
    {Metric::CpuBusiestCore, MetricGroup::Cpu, "cpu_busiest_core"},
    {Metric::CpuTemperature, MetricGroup::Cpu, "cpu_temperature"},
    {Metric::CpuClockAverage, MetricGroup::Cpu, "cpu_clock_average"},
    {Metric::CpuClockMax, MetricGroup::Cpu, "cpu_clock_max"},
    {Metric::CpuPower, MetricGroup::Cpu, "cpu_power"},
    {Metric::RamUsedGb, MetricGroup::Ram, "ram_used_gb"},
    {Metric::RamUsagePercent, MetricGroup::Ram, "ram_usage_percent"},
    {Metric::GameName, MetricGroup::Other, "game_name"},
    {Metric::WallClock, MetricGroup::Other, "wall_clock"},
}};

}  // namespace

const std::array<MetricInfo, kMetricCount>& allMetrics() {
    return kMetrics;
}

const MetricInfo& metricInfo(Metric metric) {
    return kMetrics[static_cast<size_t>(metric)];
}

std::optional<Metric> metricFromKey(std::string_view key) {
    for (const auto& info : kMetrics) {
        if (info.key == key) {
            return info.metric;
        }
    }
    return std::nullopt;
}

std::string_view groupKey(MetricGroup group) {
    switch (group) {
        case MetricGroup::Fps: return "fps";
        case MetricGroup::Gpu: return "gpu";
        case MetricGroup::Vram: return "vram";
        case MetricGroup::Cpu: return "cpu";
        case MetricGroup::Ram: return "ram";
        case MetricGroup::Other: return "other";
        case MetricGroup::Count: break;
    }
    return "other";
}

MetricSet MetricSet::fpsOnly() {
    MetricSet set;
    set.set(Metric::Fps);
    set.set(Metric::Frametime);
    set.set(Metric::OnePercentLow);
    set.set(Metric::FrametimeGraph);
    set.set(Metric::GameName);
    return set;
}

// Mirrors the "PADRÃO" preset from the original mockup.
MetricSet MetricSet::defaults() {
    MetricSet set;
    set.set(Metric::Fps);
    set.set(Metric::GpuUsage);
    set.set(Metric::GpuTemperature);
    set.set(Metric::GpuPower);
    set.set(Metric::VramUsedGb);
    set.set(Metric::CpuUsage);
    set.set(Metric::CpuTemperature);
    set.set(Metric::CpuPower);
    set.set(Metric::RamUsedGb);
    set.set(Metric::GameName);
    return set;
}

MetricSet MetricSet::everything() {
    MetricSet set;
    for (const auto& info : kMetrics) {
        set.set(info.metric);
    }
    return set;
}

}  // namespace perfpulse::core
