#pragma once

#include <windows.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "app/settings_window.hpp"
#include "app/tray_icon.hpp"
#include "capture/frame_capture.hpp"
#include "core/config.hpp"
#include "detection/game_detector.hpp"
#include "report/report_generator.hpp"
#include "session/session_recorder.hpp"

namespace perfpulse::app {

// Window class of the hidden host window, and the messages another
// PerfPulse instance may post to it.
inline constexpr wchar_t kHostClassName[] = L"PerfPulseHost";
inline constexpr UINT kShowSettingsMessage = WM_APP + 10;
inline constexpr UINT kExitMessage = WM_APP + 11;

// Owns the tray icon and orchestrates the whole lifecycle:
//   idle (tray) -> game detected -> recording -> game closed -> report.
// Runs on the UI thread; heavy work (report generation, source probing) is
// moved to worker threads that post results back as window messages.
class Application {
public:
    explicit Application(HINSTANCE instance);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize(bool launchedAtLogon);
    int run();

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void onTick();
    void onTrayEvent(UINT event, POINT anchor);
    void showMenu(POINT anchor);
    void onCommand(UINT id);

    void setMonitoring(bool enabled);
    void startCapture();
    void finishSession(bool synchronous);
    void startReportJob(session::SessionData data, bool synchronous);
    void onReportReady(std::optional<report::GeneratedReport> report);
    void openSettings();
    void applySettings(const core::Config& config);
    void probeSources();
    std::string sourcesLine() const;
    void updateTray();
    void openPath(const std::filesystem::path& path) const;
    void quit();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    core::Config config_;
    TrayIcon tray_;
    capture::FrameCapture capture_;
    detection::GameDetector detector_;
    session::SessionRecorder recorder_;
    std::unique_ptr<SettingsWindow> settings_;

    std::thread reportThread_;
    bool generatingReport_ = false;
    std::filesystem::path lastReport_;
    std::filesystem::path balloonTarget_;

    std::thread probeThread_;
    mutable std::mutex sourcesMutex_;
    std::string sourcesSummary_;

    double clockSec_ = 0.0;
    bool quitting_ = false;
};

}  // namespace perfpulse::app
