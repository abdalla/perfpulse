// A stand-in "game" for end-to-end testing of PerfPulse: a borderless window
// that presents through a DXGI flip-model swap chain at a target frame rate,
// with periodic hitches, and exits after a fixed duration.
//
//   perfpulse_fake_game.exe [seconds=45] [fps=144] [--windowed]

#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <chrono>
#include <cmath>
#include <cwchar>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace {

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY || (message == WM_KEYDOWN && wParam == VK_ESCAPE)) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void bringToFront(HWND window) {
    // Foreground lock workaround for a process started from a background shell.
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_MENU;
    SendInput(1, &input, sizeof(input));
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(input));
    SetForegroundWindow(window);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const double seconds = argc > 1 ? _wtof(argv[1]) : 45.0;
    const double targetFps = argc > 2 ? _wtof(argv[2]) : 144.0;
    const bool windowed = argc > 3 && std::wcscmp(argv[3], L"--windowed") == 0;
    LocalFree(argv);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"PerfPulseFakeGame";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&windowClass);

    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor);
    RECT r = monitor.rcMonitor;
    if (windowed) {
        r = {r.left + 100, r.top + 100, r.left + 900, r.top + 600};
    }
    HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"PerfPulse Fake Game", WS_POPUP | WS_VISIBLE,
                                  r.left, r.top, r.right - r.left, r.bottom - r.top, nullptr, nullptr, instance,
                                  nullptr);
    bringToFront(window);

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                             D3D11_SDK_VERSION, &desc, &swapChain, &device, nullptr, &context))) {
        return 1;
    }
    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    ComPtr<ID3D11RenderTargetView> target;
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &target);

    // sleep_until() rounds to the 15.6 ms system tick; a high-resolution
    // waitable timer paces frames the way a real frame limiter does.
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    const auto sleepUntil = [timer](std::chrono::steady_clock::time_point deadline) {
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return;
        }
        LARGE_INTEGER due;
        due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100;
        SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(timer, INFINITE);
    };

    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto frameBudget = std::chrono::duration<double>(1.0 / targetFps);
    auto next = clock::now();
    long long frame = 0;
    MSG message{};
    while (true) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                return 0;
            }
            DispatchMessageW(&message);
        }
        const double elapsed = std::chrono::duration<double>(clock::now() - start).count();
        if (elapsed >= seconds) {
            break;
        }
        const float phase = static_cast<float>(elapsed);
        const float color[4] = {0.43f + 0.2f * std::sin(phase), 0.37f, 0.97f, 1.0f};
        context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
        context->ClearRenderTargetView(target.Get(), color);
        swapChain->Present(0, 0);
        ++frame;
        // A 60 ms hitch every ~5 seconds, like shader compilation.
        if (frame % static_cast<long long>(targetFps * 5) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
        next += std::chrono::duration_cast<clock::duration>(frameBudget);
        if (next < clock::now()) {
            next = clock::now();  // fell behind (hitch): do not burst to catch up
        }
        sleepUntil(next);
    }
    CloseHandle(timer);
    DestroyWindow(window);
    return 0;
}
