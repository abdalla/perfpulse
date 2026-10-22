#include "report/demo_session.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace perfpulse::report {

namespace {

struct Phase {
    double endSec;
    double fps;         // target average
    double fpsNoise;    // relative frame-to-frame noise
    float gpuUsage;
    float busiestThread;
    bool foreground;
    bool capped;  // a frame limiter holds FPS steady: no scene drift
};

// Timeline of the demo session.
constexpr Phase kPhases[] = {
    {20, 3, 0.6, 35, 40, true, false},        // loading screen
    {420, 92, 0.07, 98, 68, true, false},     // open city at 4K: GPU-bound
    {470, 60, 0.05, 20, 15, false, false},    // alt-tab to Discord
    {780, 64, 0.12, 74, 96, true, false},     // crowded market: main thread saturated
    {1080, 141, 0.015, 71, 62, true, true},   // indoor area: Reflex cap at 141 FPS
};

const Phase& phaseAt(double t) {
    for (const auto& phase : kPhases) {
        if (t < phase.endSec) {
            return phase;
        }
    }
    return kPhases[std::size(kPhases) - 1];
}

}  // namespace

session::SessionData makeDemoSession(const core::MetricSet& metrics) {
    using namespace std::chrono;
    std::mt19937 random(20261022);
    std::normal_distribution<double> gauss(0.0, 1.0);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    session::SessionData data;
    data.metrics = metrics;
    data.sampleIntervalSec = 1.0;
    const double duration = kPhases[std::size(kPhases) - 1].endSec;
    data.durationSec = duration;

    auto& info = data.info;
    info.gameName = "Cyberpunk 2077 (demo)";
    info.exeName = "cyberpunk2077.exe";
    info.exePath = "C:\\Games\\Cyberpunk 2077\\bin\\x64\\Cyberpunk2077.exe";
    info.windowTitle = "Cyberpunk 2077 (C) 2020 by CD Projekt RED";
    info.presentApi = "DXGI";
    info.dominantSyncInterval = 0;
    info.monitorName = "LG ULTRAGEAR";
    info.monitorRefreshHz = 144;
    info.monitorWidth = 3840;
    info.monitorHeight = 2160;
    info.endUnixMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    info.startUnixMs = info.endUnixMs - static_cast<int64_t>(duration * 1000.0);
    info.gpuSource = "NVML + NVAPI";
    info.cpuSensorSource = "PawnIO (IntelMSR)";
    auto& hw = info.hardware;
    hw.cpuName = "Intel(R) Core(TM) i9-14900K";
    hw.cpuPerformanceCores = 8;
    hw.cpuEfficiencyCores = 16;
    hw.cpuLogicalProcessors = 32;
    hw.cpuTjMax = 100.0f;
    hw.gpuName = "NVIDIA GeForce RTX 3080 Ti";
    hw.gpuDriver = "581.57";
    hw.gpuPowerLimitW = 350.0f;
    hw.ramTotalGb = 64.0f;
    hw.osVersion = "Windows 11 Pro 25H2 (build 26300.1)";

    // Frames
    double t = 0.0;
    double drift = 0.0;
    while (t < duration) {
        const auto& phase = phaseAt(t);
        drift = 0.995 * drift + 0.005 * gauss(random);  // slow scene-complexity changes
        const double sceneDrift = phase.capped ? 0.0 : drift * 0.8;
        double ms = 1000.0 / phase.fps * (1.0 + sceneDrift + gauss(random) * phase.fpsNoise);
        // Shader compilation hitches, mostly in the market.
        const double hitchChance = (t > 470 && t < 780) ? 0.0009 : (t > 20 && t < 420 ? 0.00015 : 0.0);
        if (uniform(random) < hitchChance) {
            ms = 35.0 + uniform(random) * 90.0;
        }
        ms = std::clamp(ms, 2.0, 400.0);
        t += ms / 1000.0;
        data.frameTimesSec.push_back(t);
    }

    // Sensors
    float vram = 8.4f;
    for (int second = 1; second <= static_cast<int>(duration); ++second) {
        const auto& phase = phaseAt(second - 0.5);
        session::SensorSample s;
        s.timeSec = second;
        s.gameInForeground = phase.foreground;
        const auto jitter = [&](float base, float spread) {
            return base + static_cast<float>(gauss(random)) * spread;
        };
        s.gpuUsage = std::clamp(jitter(phase.gpuUsage, phase.gpuUsage > 95 ? 0.8f : 4.0f), 0.0f, 100.0f);
        const float load = s.gpuUsage / 100.0f;
        s.gpuPower = jitter(60.0f + 285.0f * load, 6.0f);
        s.gpuTemperature = jitter(48.0f + 26.0f * load, 0.6f);
        s.gpuHotspot = s.gpuTemperature + jitter(12.0f, 0.8f);
        s.gpuCoreClock = jitter(load > 0.9f ? 1830.0f : 1905.0f, 12.0f);
        s.gpuMemoryClock = 9501.0f;
        s.gpuVoltage = jitter(load > 0.9f ? 0.981f : 0.9f, 0.006f);
        s.gpuFan = jitter(35.0f + 40.0f * load, 1.0f);
        s.gpuLimiters = load > 0.95f ? session::kLimiterPower : session::kLimiterNone;
        if (phase.foreground && second > 20) {
            vram = std::min(11.2f, vram + 0.004f);
        }
        s.vramUsedGb = jitter(vram, 0.05f);
        s.vramTotalGb = 12.0f;
        s.vramTemperature = jitter(60.0f + 30.0f * load, 0.8f);

        const float thread = phase.busiestThread / 100.0f;
        s.gameBusiestThread = std::clamp(jitter(phase.busiestThread, 2.0f), 0.0f, 100.0f);
        s.cpuUsage = std::clamp(jitter(14.0f + 30.0f * thread, 2.0f), 0.0f, 100.0f);
        s.cpuBusiestCore = std::clamp(s.gameBusiestThread + jitter(2.0f, 1.0f), 0.0f, 100.0f);
        s.gameCpuUsage = s.cpuUsage * 0.8f;
        s.cpuPower = jitter(75.0f + 110.0f * thread, 5.0f);
        s.cpuTemperature = jitter(52.0f + 30.0f * thread, 1.5f);
        s.cpuClockAverage = jitter(4700.0f, 60.0f);
        s.cpuClockMax = jitter(5690.0f, 20.0f);
        s.ramUsedGb = jitter(21.5f, 0.2f);
        s.ramTotalGb = 63.8f;
        s.gameWorkingSetGb = jitter(9.1f, 0.1f);
        data.samples.push_back(s);
    }
    return data;
}

}  // namespace perfpulse::report
