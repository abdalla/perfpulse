#pragma once

#include <windows.h>

namespace perfpulse::app {

enum class TrayState {
    Paused,     // monitoring disabled: grey
    Waiting,    // armed, no game: brand gradient
    Detecting,  // candidate being confirmed: amber pulse
    Recording,  // recording a session: gradient with a red dot
    Warning,    // ETW unavailable: grey with an amber dot
};

// Draws the tray icon at the current small-icon size. The caller owns the
// returned HICON (DestroyIcon).
HICON createTrayIcon(TrayState state, int size);

}  // namespace perfpulse::app
