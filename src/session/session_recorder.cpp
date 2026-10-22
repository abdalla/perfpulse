#include "session/session_recorder.hpp"

#include <algorithm>
#include <chrono>

#include "core/log.hpp"
#include "detection/monitor_info.hpp"
#include "sensors/sensor_hub.hpp"

namespace perfpulse::session {

namespace {

int64_t qpcNow() {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

double qpcFrequency() {
    static const double frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return static_cast<double>(value.QuadPart);
    }();
    return frequency;
}

int64_t unixMillisNow() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool isForeground(DWORD processId) {
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    return foregroundPid == processId;
}

}  // namespace

SessionRecorder::~SessionRecorder() {
    if (sampler_.joinable()) {
        SetEvent(stopEvent_.get());
        sampler_.join();
    }
}

bool SessionRecorder::start(const detection::GameCandidate& game, const core::Config& config,
                            capture::FrameCapture& capture) {
    if (recording_) {
        return false;
    }
    process_.reset(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, game.process.processId));
    if (!process_) {
        core::log::warn("Cannot open game process " + std::to_string(game.process.processId));
        return false;
    }
    stopEvent_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    processId_ = game.process.processId;

    data_ = SessionData{};
    auto& info = data_.info;
    info.gameName = game.gameName;
    info.exeName = game.process.exeName;
    info.exePath = game.process.exePath;
    info.windowTitle = game.windowTitle;
    info.startUnixMs = unixMillisNow();
    const auto monitor = detection::describeMonitor(game.monitor);
    info.monitorName = monitor.friendlyName;
    info.monitorRefreshHz = monitor.refreshHz;
    info.monitorWidth = monitor.width();
    info.monitorHeight = monitor.height();
    data_.metrics = config.metrics;
    data_.sampleIntervalSec = config.sampleIntervalMs / 1000.0;

    originQpc_ = qpcNow();
    capture.beginRecording(processId_, originQpc_);
    recording_ = true;
    sampler_ = std::thread(&SessionRecorder::samplerLoop, this, config.metrics, config.sampleIntervalMs);
    core::log::info("Recording started: " + info.gameName + " (" + info.exeName + ")");
    return true;
}

SessionData SessionRecorder::stop(capture::FrameCapture& capture) {
    if (sampler_.joinable()) {
        SetEvent(stopEvent_.get());
        sampler_.join();
    }
    auto frames = capture.endRecording();
    recording_ = false;

    data_.frameTimesSec = std::move(frames.timesSec);
    data_.info.presentApi = std::string(capture::presentSourceName(frames.source));
    data_.info.dominantSyncInterval = frames.dominantSyncInterval;
    data_.info.etwEventsLost = capture.eventsLost();
    data_.info.endUnixMs = unixMillisNow();
    data_.durationSec = static_cast<double>(qpcNow() - originQpc_) / qpcFrequency();
    if (data_.frameTimesSec.empty()) {
        data_.info.presentApi.clear();
    } else {
        // The exit is noticed on the next detector tick: drop the samples taken
        // after the game's last frame so they do not count as background time.
        const double end = data_.frameTimesSec.back() + data_.sampleIntervalSec;
        while (!data_.samples.empty() && data_.samples.back().timeSec > end) {
            data_.samples.pop_back();
        }
        data_.durationSec = std::min(data_.durationSec, end);
    }
    process_.reset();
    processId_ = 0;
    core::log::info("Recording stopped: " + std::to_string(data_.frameTimesSec.size()) + " frames, " +
                    std::to_string(data_.samples.size()) + " samples");
    return std::move(data_);
}

bool SessionRecorder::gameExited() const {
    return process_ && WaitForSingleObject(process_.get(), 0) == WAIT_OBJECT_0;
}

double SessionRecorder::elapsedSec() const {
    return recording_ ? static_cast<double>(qpcNow() - originQpc_) / qpcFrequency() : 0.0;
}

void SessionRecorder::samplerLoop(core::MetricSet metrics, int intervalMs) {
    SetThreadDescription(GetCurrentThread(), L"PerfPulse sampler");
    // Sensor backends are created here so COM, PDH and driver handles live on this thread.
    sensors::SensorHub hub;
    hub.describe(data_.info.hardware, data_.info.gpuSource, data_.info.cpuSensorSource);
    const auto request = sensors::makeSensorRequest(metrics, processId_);

    const double frequency = qpcFrequency();
    data_.samples.reserve(4096);
    for (int64_t tick = 1;; ++tick) {
        // Absolute schedule: sensor latency does not accumulate as drift.
        const int64_t dueQpc = originQpc_ + static_cast<int64_t>(tick * intervalMs * frequency / 1000.0);
        const int64_t waitMs =
            std::max<int64_t>(0, static_cast<int64_t>((dueQpc - qpcNow()) * 1000.0 / frequency));
        if (WaitForSingleObject(stopEvent_.get(), static_cast<DWORD>(waitMs)) == WAIT_OBJECT_0) {
            break;
        }
        SensorSample sample;
        hub.read(sample, request);
        sample.timeSec = static_cast<double>(qpcNow() - originQpc_) / frequency;
        sample.gameInForeground = isForeground(processId_);
        data_.samples.push_back(sample);
    }
}

}  // namespace perfpulse::session
