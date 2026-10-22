#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "session/session_data.hpp"

namespace perfpulse::analysis {

enum class Bottleneck {
    Gpu,              // GPU saturated: the healthy state for most games.
    GpuThrottling,    // GPU saturated while held back by temperature/hardware slowdown.
    Cpu,              // GPU waiting on the CPU (often a single saturated game thread).
    CpuThermal,       // CPU-bound while the package is at its thermal limit.
    Vram,             // Video memory full: textures spill to system RAM.
    Ram,              // System memory nearly exhausted.
    FrameCap,         // Frame limiter or V-Sync holding FPS below what the PC can do.
    Mixed,            // Nothing saturated (engine limits, streaming, synchronization).
    Loading,          // Almost no frames: loading screen or menu transition.
    Background,       // Game not in the foreground.
    Count,
};

inline constexpr size_t kBottleneckCount = static_cast<size_t>(Bottleneck::Count);

std::string_view bottleneckKey(Bottleneck bottleneck);

struct SampleFrameStats {
    float fps = 0.0f;
    float maxFrametimeMs = 0.0f;
};

struct BottleneckSummary {
    std::vector<Bottleneck> timeline;  // One entry per sensor sample.
    std::array<double, kBottleneckCount> shareOfActiveTime{};  // 0..1, excludes Background/Loading.
    Bottleneck offender = Bottleneck::Mixed;
    double offenderShare = 0.0;
    float detectedCapFps = 0.0f;   // 0 when no cap plateau was found.
    // Rough FPS the system could reach without the cap, from GPU headroom.
    float uncappedEstimateFps = 0.0f;
    // Averages while CPU-bound, used to explain the verdict.
    float cpuBoundBusiestThread = session::kNoValue;
    float cpuBoundGpuUsage = session::kNoValue;
    double gpuPowerLimitedShare = 0.0;  // share of GPU-bound time with the power limiter active
};

struct BottleneckThresholds {
    float gpuSaturated = 95.0f;     // % GPU usage considered saturated
    float gpuIdleForCpu = 85.0f;    // GPU below this hints at a CPU bottleneck
    float threadSaturated = 85.0f;  // % of one core for the game's busiest thread
    float coreSaturated = 95.0f;    // % for the busiest logical processor
    float cpuTotalSaturated = 90.0f;
    float vramFull = 0.97f;         // fraction of VRAM in use
    float ramFull = 0.95f;          // fraction of RAM in use
    float loadingFps = 8.0f;
    float capTolerance = 0.02f;     // ±2 % around the cap value
    float capMinShare = 0.20f;      // share of active samples that must sit on the plateau
    float cpuThermalMarginC = 3.0f; // within this of TjMax
};

BottleneckSummary analyzeBottlenecks(const std::vector<session::SensorSample>& samples,
                                     const std::vector<SampleFrameStats>& frameStats, int refreshRateHz,
                                     float cpuTjMax, const BottleneckThresholds& thresholds = {});

}  // namespace perfpulse::analysis
