#include "app/theme.hpp"

#include <dwmapi.h>
#include <uxtheme.h>

#include <iterator>
#include <map>
#include <string_view>

namespace perfpulse::app {

namespace {

constexpr Palette kLight = {
    RGB(243, 243, 246),  // background
    RGB(232, 232, 236),  // surface
    RGB(214, 214, 220),  // border
    RGB(28, 28, 32),     // text
    RGB(104, 104, 114),  // mutedText
    RGB(109, 94, 247),   // accent
    RGB(255, 255, 255),  // accentText
    RGB(34, 34, 40),     // toggleOn (dark pill, like the mockup)
    RGB(255, 255, 255),  // toggleOff
    RGB(255, 255, 255),  // knob
    RGB(255, 255, 255),  // inputBackground
};

constexpr Palette kDark = {
    RGB(24, 24, 30),     // background
    RGB(34, 34, 42),     // surface
    RGB(52, 52, 64),     // border
    RGB(236, 236, 242),  // text
    RGB(150, 150, 165),  // mutedText
    RGB(124, 106, 255),  // accent
    RGB(255, 255, 255),  // accentText
    RGB(124, 106, 255),  // toggleOn
    RGB(58, 58, 70),     // toggleOff
    RGB(245, 245, 250),  // knob
    RGB(44, 44, 54),     // inputBackground
};

bool gDark = false;
bool gResolved = false;

}  // namespace

bool systemUsesDarkMode() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value == 0;
}

const Palette& palette() {
    if (!gResolved) {
        refreshTheme();
    }
    return gDark ? kDark : kLight;
}

void refreshTheme() {
    gDark = systemUsesDarkMode();
    gResolved = true;
}

void applyWindowTheme(HWND window) {
    const BOOL dark = gDark ? TRUE : FALSE;
    constexpr DWORD kUseImmersiveDarkMode = 20;  // DWMWA_USE_IMMERSIVE_DARK_MODE
    DwmSetWindowAttribute(window, kUseImmersiveDarkMode, &dark, sizeof(dark));
    // Rounded corners and Mica-like caption on Windows 11; ignored elsewhere.
    const DWORD corner = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(window, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
    const COLORREF caption = palette().background;
    DwmSetWindowAttribute(window, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
}

void applyControlTheme(HWND control) {
    wchar_t className[32];
    GetClassNameW(control, className, static_cast<int>(std::size(className)));
    const std::wstring_view name(className);
    if (!gDark) {
        SetWindowTheme(control, L"Explorer", nullptr);
        return;
    }
    SetWindowTheme(control, name == L"ComboBox" ? L"DarkMode_CFD" : L"DarkMode_Explorer", nullptr);
}

void enableDarkMenus() {
    // uxtheme ordinal 135: SetPreferredAppMode(AllowDark = 1). Used by many
    // Win32 apps (Notepad++, Explorer itself); absent on old builds.
    using SetPreferredAppModeFn = int(WINAPI*)(int);
    using FlushMenuThemesFn = void(WINAPI*)();
    HMODULE uxtheme = GetModuleHandleW(L"uxtheme.dll");
    if (uxtheme == nullptr) {
        uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (uxtheme == nullptr) {
        return;
    }
    const auto setMode = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
    const auto flush = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
    if (setMode != nullptr) {
        setMode(1);
    }
    if (flush != nullptr) {
        flush();
    }
}

HBRUSH brush(COLORREF color) {
    static std::map<COLORREF, HBRUSH> brushes;
    auto& entry = brushes[color];
    if (entry == nullptr) {
        entry = CreateSolidBrush(color);
    }
    return entry;
}

}  // namespace perfpulse::app
