#include "detection/game_detector.hpp"

#include <shellapi.h>

#include <algorithm>

#include "core/strings.hpp"

namespace perfpulse::detection {

namespace {

std::string monitorDeviceName(HMONITOR monitor) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    return GetMonitorInfoW(monitor, &info) ? core::toUtf8(info.szDevice) : std::string();
}

bool isShellWindow(HWND window) {
    if (window == GetShellWindow() || window == GetDesktopWindow()) {
        return true;
    }
    wchar_t className[64];
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    const std::wstring_view name(className);
    return name == L"Progman" || name == L"WorkerW" || name == L"Shell_TrayWnd";
}

bool isExclusiveFullscreen() {
    QUERY_USER_NOTIFICATION_STATE state{};
    return SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

}  // namespace

bool GameDetector::coversMonitor(HWND window, HMONITOR monitor) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT windowRect{};
    if (!GetMonitorInfoW(monitor, &info) || !GetWindowRect(window, &windowRect)) {
        return false;
    }
    RECT overlap{};
    if (!IntersectRect(&overlap, &windowRect, &info.rcMonitor)) {
        return false;
    }
    const auto area = [](const RECT& r) {
        return static_cast<double>(r.right - r.left) * static_cast<double>(r.bottom - r.top);
    };
    return area(overlap) >= area(info.rcMonitor) * 0.95;
}

std::optional<GameCandidate> GameDetector::tick(const core::Config& config,
                                                const std::vector<capture::ProcessActivity>& activity,
                                                double nowSec, double tickIntervalSec) {
    const auto miss = [this] {
        if (++missedTicks_ > kMaxMissedTicks) {
            reset();
        }
        return std::nullopt;
    };

    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || isShellWindow(foreground)) {
        return miss();
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return miss();
    }

    if (!pending_ || pending_->process.processId != pid) {
        GameCandidate candidate;
        candidate.process = queryProcess(pid);
        if (candidate.process.exePath.empty()) {
            return miss();
        }
        candidate.window = foreground;
        pending_ = std::move(candidate);
        pendingSinceSec_ = nowSec;
        missedTicks_ = 0;
    }
    auto& candidate = *pending_;
    candidate.window = foreground;
    candidate.monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);

    const auto& exe = candidate.process.exeName;
    const bool allowed = config.isAlwaysMonitored(exe);
    if (!allowed && config.isNeverMonitored(exe)) {
        reset();
        return std::nullopt;
    }
    if (!config.monitorDevice.empty() && monitorDeviceName(candidate.monitor) != config.monitorDevice) {
        return miss();
    }

    if (!allowed) {
        if (config.requireFullscreen && !coversMonitor(foreground, candidate.monitor) && !isExclusiveFullscreen()) {
            return miss();
        }
        const auto it = std::find_if(activity.begin(), activity.end(),
                                     [pid](const capture::ProcessActivity& a) { return a.processId == pid; });
        const double presentsPerSecond =
            it == activity.end() ? 0.0 : static_cast<double>(it->presents) / std::max(tickIntervalSec, 0.1);
        if (presentsPerSecond < kMinPresentsPerSecond) {
            return miss();
        }
    }
    missedTicks_ = 0;

    const double required = allowed ? 0.0 : static_cast<double>(config.detectionDelaySec);
    if (nowSec - pendingSinceSec_ < required) {
        return std::nullopt;
    }
    candidate.windowTitle = windowTitle(foreground);
    candidate.gameName = resolveGameName(candidate.process, candidate.windowTitle);
    auto confirmed = std::move(candidate);
    reset();
    return confirmed;
}

void GameDetector::reset() {
    pending_.reset();
    missedTicks_ = 0;
}

}  // namespace perfpulse::detection
