#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <filesystem>
#include <string_view>
#include <vector>

#include "app/application.hpp"
#include "app/diagnostics.hpp"
#include "app/gdiplus_include.hpp"
#include "core/i18n.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

namespace {

// Hints the scheduler that PerfPulse is a background utility: on hybrid CPUs
// (Intel 12th gen+) its threads prefer E-cores and leave P-cores to the game.
void enableEfficiencyMode() {
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
}

struct Arguments {
    bool autostart = false;
    bool diagnostics = false;
    bool demoReport = false;
    bool exitRunning = false;
    std::filesystem::path output;
};

Arguments parseArguments() {
    Arguments args;
    int count = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 1; i < count; ++i) {
        const std::wstring_view arg(argv[i]);
        if (arg == L"--autostart") {
            args.autostart = true;
        } else if (arg == L"--diag") {
            args.diagnostics = true;
        } else if (arg == L"--demo-report") {
            args.demoReport = true;
        } else if (arg == L"--exit") {
            args.exitRunning = true;
        } else if (arg == L"--out" && i + 1 < count) {
            args.output = argv[++i];
        }
    }
    LocalFree(argv);
    return args;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    using namespace perfpulse;
    const auto args = parseArguments();
    core::log::initialize();

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return 1;
    }
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    int exitCode = 0;
    if (args.diagnostics) {
        exitCode = app::runDiagnostics(args.output);
    } else if (args.demoReport) {
        exitCode = app::runDemoReport(args.output);
    } else if (args.exitRunning) {
        // Asks a running instance to finish its session report and quit.
        if (HWND existing = FindWindowW(app::kHostClassName, nullptr)) {
            PostMessageW(existing, app::kExitMessage, 0, 0);
        }
    } else {
        // One instance per user session; a second launch opens the settings of the first.
        // A non-elevated launch gets ACCESS_DENIED on the elevated instance's mutex.
        HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\PerfPulse.SingleInstance");
        const DWORD mutexError = GetLastError();
        if (mutexError == ERROR_ALREADY_EXISTS || mutexError == ERROR_ACCESS_DENIED) {
            if (HWND existing = FindWindowW(app::kHostClassName, nullptr)) {
                PostMessageW(existing, app::kShowSettingsMessage, 0, 0);
            }
        } else {
            enableEfficiencyMode();
            INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
            InitCommonControlsEx(&controls);
            app::Application application(instance);
            if (application.initialize(args.autostart)) {
                exitCode = application.run();
            } else {
                core::log::error("Initialization failed");
                exitCode = 1;
            }
        }
        if (mutex != nullptr) {
            CloseHandle(mutex);
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return exitCode;
}
