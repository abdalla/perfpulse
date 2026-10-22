#pragma once

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

#include "capture/frame_capture.hpp"
#include "core/config.hpp"
#include "core/win_handle.hpp"
#include "detection/game_detector.hpp"
#include "session/session_data.hpp"

namespace perfpulse::session {

// Records one game session: frame timestamps come from FrameCapture, sensor
// samples from a dedicated sampler thread that exists only while recording.
// All data stays in memory until stop(); nothing touches the disk mid-game.
class SessionRecorder {
public:
    SessionRecorder() = default;
    ~SessionRecorder();
    SessionRecorder(const SessionRecorder&) = delete;
    SessionRecorder& operator=(const SessionRecorder&) = delete;

    bool start(const detection::GameCandidate& game, const core::Config& config, capture::FrameCapture& capture);
    SessionData stop(capture::FrameCapture& capture);

    bool isRecording() const { return recording_; }
    bool gameExited() const;
    DWORD processId() const { return processId_; }
    const std::string& gameName() const { return data_.info.gameName; }
    double elapsedSec() const;

private:
    void samplerLoop(core::MetricSet metrics, int intervalMs);

    std::atomic<bool> recording_{false};
    DWORD processId_ = 0;
    core::UniqueHandle process_;
    core::UniqueHandle stopEvent_;
    std::thread sampler_;
    int64_t originQpc_ = 0;
    SessionData data_;
};

}  // namespace perfpulse::session
