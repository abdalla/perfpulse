#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "core/metrics.hpp"

namespace perfpulse::session {

inline constexpr float kNoValue = std::numeric_limits<float>::quiet_NaN();

inline bool hasValue(float value) {
    return !std::isnan(value);
}

// Reasons reported by the GPU driver for holding clocks back.
enum GpuLimiter : uint32_t {
    kLimiterNone = 0,
    kLimiterPower = 1u << 0,
    kLimiterThermal = 1u << 1,
    kLimiterHardwareSlowdown = 1u << 2,
    kLimiterIdle = 1u << 3,
    kLimiterSync = 1u << 4,
    kLimiterUnknown = 1u << 31,  // Limiter data not available.
};

// One reading of every sensor, taken every Config::sampleIntervalMs.
// Missing readings are NaN so the report can show "N/A" instead of zero.
struct SensorSample {
    double timeSec = 0.0;  // Seconds since the session started.
    bool gameInForeground = true;

    float gpuUsage = kNoValue;         // %
    float gpuTemperature = kNoValue;   // °C
    float gpuHotspot = kNoValue;       // °C
    float gpuCoreClock = kNoValue;     // MHz
    float gpuMemoryClock = kNoValue;   // MHz
    float gpuPower = kNoValue;         // W
    float gpuVoltage = kNoValue;       // V
    float gpuFan = kNoValue;           // % (NVIDIA)
    float gpuFanRpm = kNoValue;        // RPM (WDDM drivers report RPM, not %)
    uint32_t gpuLimiters = kLimiterUnknown;

    float vramUsedGb = kNoValue;
    float vramTotalGb = kNoValue;
    float vramTemperature = kNoValue;  // °C (memory junction on GDDR6X)

    float cpuUsage = kNoValue;         // % of all logical processors
    float cpuBusiestCore = kNoValue;   // % of the busiest logical processor
    float cpuTemperature = kNoValue;   // °C package
    float cpuClockAverage = kNoValue;  // MHz
    float cpuClockMax = kNoValue;      // MHz
    float cpuPower = kNoValue;         // W package

    float gameCpuUsage = kNoValue;      // % of all logical processors used by the game
    float gameBusiestThread = kNoValue; // % of one core used by the game's busiest thread
    float gameWorkingSetGb = kNoValue;

    float ramUsedGb = kNoValue;
    float ramTotalGb = kNoValue;
};

struct HardwareInfo {
    std::string cpuName;
    int cpuPerformanceCores = 0;
    int cpuEfficiencyCores = 0;
    int cpuLogicalProcessors = 0;
    float cpuTjMax = kNoValue;
    std::string gpuName;
    std::string gpuDriver;
    float gpuPowerLimitW = kNoValue;
    float ramTotalGb = kNoValue;
    std::string osVersion;
};

struct SessionInfo {
    std::string gameName;
    std::string exeName;
    std::string exePath;
    std::string windowTitle;
    std::string presentApi;    // "DXGI", "D3D9", "DxgKrnl" or empty.
    int dominantSyncInterval = -1;  // DXGI SyncInterval, -1 when unknown.
    std::string monitorName;
    int monitorRefreshHz = 0;
    int monitorWidth = 0;
    int monitorHeight = 0;
    int64_t startUnixMs = 0;
    int64_t endUnixMs = 0;
    std::string gpuSource;     // e.g. "NVML + NVAPI"
    std::string cpuSensorSource;  // e.g. "PawnIO (IntelMSR)"
    uint64_t etwEventsLost = 0;
    HardwareInfo hardware;
};

struct SessionData {
    SessionInfo info;
    // Present timestamps in seconds since session start, ascending.
    std::vector<double> frameTimesSec;
    std::vector<SensorSample> samples;
    double sampleIntervalSec = 1.0;
    double durationSec = 0.0;
    core::MetricSet metrics;
};

}  // namespace perfpulse::session
