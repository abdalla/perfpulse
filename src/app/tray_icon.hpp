#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

#include "app/icon_factory.hpp"

namespace perfpulse::app {

// Notification-area icon. Callback messages arrive at the owner window as
// `callbackMessage` with NOTIFYICON_VERSION_4 semantics:
// LOWORD(lParam) = event, wParam = anchor point (screen coordinates).
class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool create(HWND owner, UINT callbackMessage);
    // Re-adds the icon after Explorer restarts ("TaskbarCreated").
    void recreate();
    void remove();

    void setState(TrayState state);
    void setTooltip(const std::string& text);
    void showBalloon(const std::string& title, const std::string& text, bool quiet);

private:
    void apply(DWORD message, UINT flags);

    NOTIFYICONDATAW data_{};
    HICON icon_ = nullptr;
    TrayState state_ = TrayState::Waiting;
    bool added_ = false;
};

}  // namespace perfpulse::app
