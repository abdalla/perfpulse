#pragma once

#include <array>
#include <bitset>
#include <optional>
#include <string_view>

namespace perfpulse::core {

// Every metric the user can toggle in "What to show". The order defines the
// layout inside each group of the settings window.
enum class Metric {
    // FPS
    Fps,
    Frametime,
    OnePercentLow,
    FrametimeGraph,
    // GPU
    GpuUsage,
    GpuTemperature,
    GpuHotspot,
    GpuCoreClock,
    GpuMemoryClock,
    GpuPower,
    GpuVoltage,
    GpuFan,
    GpuLimiter,
    // VRAM
    VramUsedGb,
    VramUsagePercent,
    VramTemperature,
    // CPU
    CpuUsage,
    CpuBusiestCore,
    CpuTemperature,
    CpuClockAverage,
    CpuClockMax,
    CpuPower,
    // RAM
    RamUsedGb,
    RamUsagePercent,
    // Others
    GameName,
    WallClock,
    Count,
};

enum class MetricGroup { Fps, Gpu, Vram, Cpu, Ram, Other, Count };

inline constexpr size_t kMetricCount = static_cast<size_t>(Metric::Count);

struct MetricInfo {
    Metric metric;
    MetricGroup group;
    std::string_view key;  // Stable identifier used in config.json and in reports.
};

const std::array<MetricInfo, kMetricCount>& allMetrics();
const MetricInfo& metricInfo(Metric metric);
std::optional<Metric> metricFromKey(std::string_view key);
std::string_view groupKey(MetricGroup group);

class MetricSet {
public:
    bool has(Metric metric) const { return bits_.test(static_cast<size_t>(metric)); }
    void set(Metric metric, bool enabled = true) { bits_.set(static_cast<size_t>(metric), enabled); }
    void clear() { bits_.reset(); }
    bool operator==(const MetricSet&) const = default;

    static MetricSet fpsOnly();
    static MetricSet defaults();
    static MetricSet everything();

private:
    std::bitset<kMetricCount> bits_;
};

}  // namespace perfpulse::core
