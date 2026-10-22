#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

#include "capture/frame_capture.hpp"
#include "core/config.hpp"
#include "detection/process_info.hpp"

namespace perfpulse::detection {

struct GameCandidate {
    ProcessInfo process;
    HWND window = nullptr;
    HMONITOR monitor = nullptr;
    std::string windowTitle;
    std::string gameName;
};

// Decides when "a game is being played": the foreground window lives on the
// chosen monitor, covers it (fullscreen or borderless), and keeps presenting
// frames for Config::detectionDelaySec. Allow/deny lists override the rules.
//
// tick() is cheap (a handful of user32 calls) and runs every couple of
// seconds from the UI thread.
class GameDetector {
public:
    // Returns a candidate once it has been confirmed.
    std::optional<GameCandidate> tick(const core::Config& config,
                                      const std::vector<capture::ProcessActivity>& activity, double nowSec,
                                      double tickIntervalSec);
    void reset();

    // The candidate being confirmed, if any (for the tray tooltip).
    const std::optional<GameCandidate>& pending() const { return pending_; }

    static bool coversMonitor(HWND window, HMONITOR monitor);

private:
    static constexpr double kMinPresentsPerSecond = 10.0;
    static constexpr int kMaxMissedTicks = 2;

    std::optional<GameCandidate> pending_;
    double pendingSinceSec_ = 0.0;
    int missedTicks_ = 0;
};

}  // namespace perfpulse::detection
