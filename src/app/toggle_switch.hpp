#pragma once

#include <windows.h>

#include <string>

namespace perfpulse::app {

// Owner-drawn on/off switch with a label, styled after the PerfPulse mockup.
// Behaves like a checkbox: clicking (or Space) flips the state and sends
// WM_COMMAND/BN_CLICKED to the parent.
class ToggleSwitch {
public:
    static constexpr UINT kSetCheck = WM_USER + 1;  // wParam: BOOL
    static constexpr UINT kGetCheck = WM_USER + 2;

    static void registerClass(HINSTANCE instance);
    static HWND create(HWND parent, int id, const std::wstring& label, int x, int y, HFONT font, double scale);
    // Width needed for the switch plus its label.
    static int measure(HWND parent, const std::wstring& label, HFONT font, double scale);

    static bool checked(HWND toggle);
    static void setChecked(HWND toggle, bool checked);

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

}  // namespace perfpulse::app
