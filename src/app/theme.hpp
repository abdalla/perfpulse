#pragma once

#include <windows.h>

namespace perfpulse::app {

struct Palette {
    COLORREF background;
    COLORREF surface;
    COLORREF border;
    COLORREF text;
    COLORREF mutedText;
    COLORREF accent;
    COLORREF accentText;
    COLORREF toggleOn;
    COLORREF toggleOff;
    COLORREF knob;
    COLORREF inputBackground;
};

// Follows the Windows "app mode" (Settings > Personalization > Colors).
bool systemUsesDarkMode();
const Palette& palette();
// Re-reads the system setting; call on WM_SETTINGCHANGE("ImmersiveColorSet").
void refreshTheme();

// Dark title bar and dark common controls for a top-level window.
void applyWindowTheme(HWND window);
void applyControlTheme(HWND control);
// Lets popup menus follow dark mode (undocumented uxtheme API, guarded).
void enableDarkMenus();

// Cached solid brushes; owned by the theme module.
HBRUSH brush(COLORREF color);

}  // namespace perfpulse::app
