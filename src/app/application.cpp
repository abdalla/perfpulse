#include "app/application.hpp"

#include <shellapi.h>

#include <cmath>
#include <cstdio>

#include "app/autostart.hpp"
#include "app/theme.hpp"
#include "core/i18n.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"
#include "report/demo_session.hpp"
#include "sensors/sensor_hub.hpp"

namespace perfpulse::app {

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kReportReadyMessage = WM_APP + 2;
constexpr UINT kSourcesReadyMessage = WM_APP + 3;
constexpr UINT kTickTimer = 1;
constexpr UINT kTickIntervalMs = 2000;

enum MenuId : UINT {
    kMenuToggleMonitoring = 100,
    kMenuStopNow,
    kMenuSettings,
    kMenuLastReport,
    kMenuHistory,
    kMenuOpenFolder,
    kMenuDemoReport,
    kMenuExit,
};

std::string formatDuration(double seconds) {
    const auto total = static_cast<long long>(seconds);
    char text[32];
    if (total >= 3600) {
        std::snprintf(text, sizeof(text), "%lld:%02lld:%02lld", total / 3600, (total / 60) % 60, total % 60);
    } else {
        std::snprintf(text, sizeof(text), "%lld:%02lld", total / 60, total % 60);
    }
    return text;
}

std::string formatDurationWords(double seconds) {
    const auto minutes = static_cast<long long>(std::lround(seconds / 60.0));
    if (minutes < 60) {
        return std::to_string(std::max(1LL, minutes)) + " min";
    }
    return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

double nowSec() {
    return static_cast<double>(GetTickCount64()) / 1000.0;
}

}  // namespace

Application::Application(HINSTANCE instance) : instance_(instance) {}

Application::~Application() {
    if (reportThread_.joinable()) {
        reportThread_.join();
    }
    if (probeThread_.joinable()) {
        probeThread_.join();
    }
    capture_.stop();
}

bool Application::initialize(bool launchedAtLogon) {
    config_ = core::loadConfig();
    core::setLanguage(config_.language);
    refreshTheme();
    enableDarkMenus();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &Application::windowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kHostClassName;
    if (!RegisterClassExW(&windowClass)) {
        return false;
    }
    SettingsWindow::registerClass(instance_);

    // A hidden top-level window (not message-only) so it receives the
    // "TaskbarCreated" broadcast when Explorer restarts.
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kHostClassName, L"PerfPulse", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                              instance_, this);
    if (window_ == nullptr) {
        return false;
    }
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    // PerfPulse runs elevated: let Explorer and non-elevated instances reach it through UIPI.
    for (const UINT message : {taskbarCreatedMessage_, kTrayMessage, kShowSettingsMessage, kExitMessage}) {
        ChangeWindowMessageFilterEx(window_, message, MSGFLT_ALLOW, nullptr);
    }
    tray_.create(window_, kTrayMessage);

    if (config_.monitoringEnabled) {
        startCapture();
    }
    probeSources();
    SetTimer(window_, kTickTimer, kTickIntervalMs, nullptr);
    updateTray();

    // First run: open the settings so the user sees what PerfPulse does.
    std::error_code error;
    if (!launchedAtLogon && !std::filesystem::exists(core::configFilePath(), error)) {
        core::saveConfig(config_);
        openSettings();
    }
    core::log::info("PerfPulse started");
    return true;
}

int Application::run() {
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (settings_ && settings_->isOpen() && IsDialogMessageW(settings_->handle(), &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Application::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    Application* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    } else {
        self = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return self != nullptr ? self->handleMessage(message, wParam, lParam)
                           : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_ != 0) {
        tray_.recreate();
        updateTray();
        return 0;
    }
    switch (message) {
        case WM_TIMER:
            if (wParam == kTickTimer) {
                onTick();
            }
            return 0;
        case kTrayMessage:
            onTrayEvent(LOWORD(lParam), POINT{static_cast<short>(LOWORD(wParam)), static_cast<short>(HIWORD(wParam))});
            return 0;
        case kReportReadyMessage: {
            std::unique_ptr<std::optional<report::GeneratedReport>> result(
                reinterpret_cast<std::optional<report::GeneratedReport>*>(lParam));
            onReportReady(std::move(*result));
            return 0;
        }
        case kSourcesReadyMessage:
            if (settings_ && settings_->isOpen()) {
                settings_->setSourcesSummary(sourcesLine());
            }
            return 0;
        case kShowSettingsMessage:
            openSettings();
            return 0;
        case kExitMessage:
            quit();
            return 0;
        case WM_SETTINGCHANGE:
            if (lParam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet") {
                refreshTheme();
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam != 0 && recorder_.isRecording()) {
                finishSession(true);  // Windows is shutting down: save the report now.
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void Application::onTick() {
    clockSec_ = nowSec();
    if (recorder_.isRecording()) {
        if (recorder_.gameExited()) {
            finishSession(false);
        }
        updateTray();
        return;
    }
    if (!config_.monitoringEnabled || capture_.status() != capture::FrameCapture::Status::Running) {
        return;
    }
    const auto activity = capture_.takeActivity();
    auto confirmed = detector_.tick(config_, activity, clockSec_, kTickIntervalMs / 1000.0);
    if (confirmed && recorder_.start(*confirmed, config_, capture_)) {
        if (config_.finishAction != core::FinishAction::Silent) {
            tray_.showBalloon(std::string(core::tr("notify.recording.title")),
                              core::format(core::tr("notify.recording.body"), {confirmed->gameName}), true);
        }
    }
    updateTray();
}

void Application::onTrayEvent(UINT event, POINT anchor) {
    switch (event) {
        case WM_CONTEXTMENU:
        case NIN_KEYSELECT:
            showMenu(anchor);
            break;
        case WM_LBUTTONDBLCLK:
            openSettings();
            break;
        case NIN_BALLOONUSERCLICK:
            if (!balloonTarget_.empty()) {
                openPath(balloonTarget_);
            }
            break;
        default:
            break;
    }
}

void Application::showMenu(POINT anchor) {
    HMENU menu = CreatePopupMenu();
    const auto add = [menu](UINT id, std::string_view key, UINT flags = MF_STRING) {
        AppendMenuW(menu, flags, id, core::toWide(core::tr(key)).c_str());
    };

    std::string status;
    if (generatingReport_) {
        status = std::string(core::tr("tray.status.generating"));
    } else if (recorder_.isRecording()) {
        status = core::format(core::tr("tray.status.recording"),
                              {recorder_.gameName(), formatDuration(recorder_.elapsedSec())});
    } else if (!config_.monitoringEnabled) {
        status = std::string(core::tr("tray.status.paused"));
    } else if (capture_.status() == capture::FrameCapture::Status::AccessDenied) {
        status = std::string(core::tr("tray.status.no_etw"));
    } else {
        status = std::string(core::tr("tray.status.waiting"));
    }
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, core::toWide("PerfPulse · " + status).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(kMenuToggleMonitoring, "tray.enable", MF_STRING | (config_.monitoringEnabled ? MF_CHECKED : MF_UNCHECKED));
    if (recorder_.isRecording()) {
        add(kMenuStopNow, "tray.stop_now");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(kMenuSettings, "tray.settings");
    add(kMenuLastReport, "tray.last_report", MF_STRING | (lastReport_.empty() ? MF_GRAYED : 0));
    add(kMenuHistory, "tray.history");
    add(kMenuOpenFolder, "tray.open_folder");
    add(kMenuDemoReport, "tray.demo_report", MF_STRING | (generatingReport_ ? MF_GRAYED : 0));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(kMenuExit, "tray.exit");
    SetMenuDefaultItem(menu, kMenuSettings, FALSE);

    // Required so the menu closes when the user clicks elsewhere.
    SetForegroundWindow(window_);
    const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, anchor.x,
                                          anchor.y, window_, nullptr);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
    if (command != 0) {
        onCommand(command);
    }
}

void Application::onCommand(UINT id) {
    switch (id) {
        case kMenuToggleMonitoring:
            setMonitoring(!config_.monitoringEnabled);
            break;
        case kMenuStopNow:
            finishSession(false);
            break;
        case kMenuSettings:
            openSettings();
            break;
        case kMenuLastReport:
            openPath(lastReport_);
            break;
        case kMenuHistory: {
            const auto index = report::historyIndexPath(config_);
            std::error_code error;
            openPath(std::filesystem::exists(index, error) ? index : core::reportDirectory(config_));
            break;
        }
        case kMenuOpenFolder: {
            const auto directory = core::reportDirectory(config_);
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            openPath(directory);
            break;
        }
        case kMenuDemoReport:
            startReportJob(report::makeDemoSession(core::MetricSet::everything()), false);
            break;
        case kMenuExit:
            quit();
            break;
    }
    updateTray();
}

void Application::setMonitoring(bool enabled) {
    config_.monitoringEnabled = enabled;
    core::saveConfig(config_);
    if (enabled) {
        startCapture();
    } else {
        if (recorder_.isRecording()) {
            finishSession(false);
        }
        detector_.reset();
        capture_.stop();
    }
    updateTray();
}

void Application::startCapture() {
    if (!capture_.start() && capture_.status() == capture::FrameCapture::Status::AccessDenied) {
        tray_.showBalloon("PerfPulse", std::string(core::tr("tray.status.no_etw")), true);
    }
}

void Application::finishSession(bool synchronous) {
    if (!recorder_.isRecording()) {
        return;
    }
    auto data = recorder_.stop(capture_);
    detector_.reset();
    if (data.durationSec < config_.minSessionSec || data.frameTimesSec.size() < 2) {
        core::log::info("Session discarded (too short)");
        if (config_.finishAction != core::FinishAction::Silent) {
            balloonTarget_.clear();
            tray_.showBalloon(std::string(core::tr("notify.session_short.title")),
                              core::format(core::tr("notify.session_short.body"),
                                           {data.info.gameName, std::to_string(config_.minSessionSec)}),
                              true);
        }
        updateTray();
        return;
    }
    startReportJob(std::move(data), synchronous);
}

void Application::startReportJob(session::SessionData data, bool synchronous) {
    if (reportThread_.joinable()) {
        reportThread_.join();  // At most one report is built at a time.
    }
    generatingReport_ = true;
    updateTray();
    if (synchronous) {
        onReportReady(report::generateReport(data, config_));
        return;
    }
    reportThread_ = std::thread([this, data = std::move(data), config = config_]() {
        SetThreadDescription(GetCurrentThread(), L"PerfPulse report");
        auto result = std::make_unique<std::optional<report::GeneratedReport>>(report::generateReport(data, config));
        if (!PostMessageW(window_, kReportReadyMessage, 0, reinterpret_cast<LPARAM>(result.get()))) {
            return;
        }
        result.release();
    });
}

void Application::onReportReady(std::optional<report::GeneratedReport> generated) {
    generatingReport_ = false;
    if (!generated) {
        tray_.showBalloon(std::string(core::tr("notify.report_failed.title")), "perfpulse.log", false);
        updateTray();
        return;
    }
    lastReport_ = generated->path;
    switch (config_.finishAction) {
        case core::FinishAction::OpenReport:
            openPath(generated->path);
            break;
        case core::FinishAction::Notify: {
            balloonTarget_ = generated->path;
            char fps[16];
            std::snprintf(fps, sizeof(fps), "%.0f", generated->averageFps);
            tray_.showBalloon(core::format(core::tr("notify.report_ready.title"), {generated->gameName}),
                              core::format(core::tr("notify.report_ready.body"),
                                           {formatDurationWords(generated->durationSec), fps, generated->grade}),
                              false);
            break;
        }
        case core::FinishAction::Silent:
            break;
    }
    // Give the memory used by the session back to the system while idling.
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    updateTray();
}

void Application::openSettings() {
    if (settings_ && settings_->isOpen()) {
        settings_->show();
        return;
    }
    settings_ = std::make_unique<SettingsWindow>(instance_, config_, sourcesLine(),
                                                 [this](const core::Config& config) { applySettings(config); });
    settings_->show();
}

std::string Application::sourcesLine() const {
    std::string etw;
    switch (capture_.status()) {
        case capture::FrameCapture::Status::Running: etw = std::string(core::tr("source.etw_ok")); break;
        case capture::FrameCapture::Status::AccessDenied: etw = std::string(core::tr("source.etw_denied")); break;
        case capture::FrameCapture::Status::Failed: etw = std::string(core::tr("source.etw_failed")); break;
        case capture::FrameCapture::Status::Stopped: etw = "FPS: -"; break;
    }
    std::lock_guard lock(sourcesMutex_);
    return sourcesSummary_.empty() ? etw : etw + " · " + sourcesSummary_;
}

void Application::applySettings(const core::Config& updated) {
    const core::Config previous = config_;
    config_ = updated;
    config_.monitoringEnabled = previous.monitoringEnabled;
    if (config_.language != previous.language) {
        core::setLanguage(config_.language);
    }
    if (config_.startWithWindows != previous.startWithWindows && !setAutostart(config_.startWithWindows)) {
        config_.startWithWindows = previous.startWithWindows;
        tray_.showBalloon("PerfPulse", std::string(core::tr("settings.autostart_failed")), false);
    }
    core::saveConfig(config_);
    detector_.reset();
    updateTray();
}

void Application::probeSources() {
    // Initializing NVML/PawnIO/WMI takes a moment: do it once in the background.
    probeThread_ = std::thread([this] {
        SetThreadDescription(GetCurrentThread(), L"PerfPulse sensor probe");
        sensors::SensorHub hub;
        session::HardwareInfo hardware;
        std::string gpuSource;
        std::string cpuSource;
        hub.describe(hardware, gpuSource, cpuSource);
        std::string summary = "GPU: " + gpuSource;
        if (!hardware.gpuName.empty()) {
            summary += " (" + hardware.gpuName + ")";
        }
        summary += " · CPU: " + cpuSource;
        core::log::info("Sources: " + summary);
        {
            std::lock_guard lock(sourcesMutex_);
            sourcesSummary_ = std::move(summary);
        }
        PostMessageW(window_, kSourcesReadyMessage, 0, 0);
    });
}

void Application::updateTray() {
    std::string status;
    TrayState state = TrayState::Waiting;
    if (generatingReport_) {
        status = std::string(core::tr("tray.status.generating"));
        state = TrayState::Recording;
    } else if (recorder_.isRecording()) {
        status = core::format(core::tr("tray.status.recording"),
                              {recorder_.gameName(), formatDuration(recorder_.elapsedSec())});
        state = TrayState::Recording;
    } else if (!config_.monitoringEnabled) {
        status = std::string(core::tr("tray.status.paused"));
        state = TrayState::Paused;
    } else if (capture_.status() != capture::FrameCapture::Status::Running) {
        status = std::string(core::tr("tray.status.no_etw"));
        state = TrayState::Warning;
    } else if (const auto& pending = detector_.pending()) {
        status = core::format(core::tr("tray.status.candidate"), {pending->process.exeName});
        state = TrayState::Detecting;
    } else {
        status = std::string(core::tr("tray.status.waiting"));
    }
    tray_.setState(state);
    tray_.setTooltip("PerfPulse\n" + status);
}

void Application::openPath(const std::filesystem::path& path) const {
    if (!path.empty()) {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void Application::quit() {
    if (quitting_) {
        return;
    }
    quitting_ = true;
    KillTimer(window_, kTickTimer);
    if (recorder_.isRecording()) {
        finishSession(true);
    }
    if (reportThread_.joinable()) {
        reportThread_.join();
    }
    settings_.reset();
    tray_.remove();
    capture_.stop();
    DestroyWindow(window_);
}

}  // namespace perfpulse::app
