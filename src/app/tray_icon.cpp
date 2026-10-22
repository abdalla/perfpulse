#include "app/tray_icon.hpp"

#include "core/strings.hpp"

namespace perfpulse::app {

namespace {

constexpr UINT kIconId = 1;

int trayIconSize(HWND owner) {
    const UINT dpi = GetDpiForWindow(owner);
    return GetSystemMetricsForDpi(SM_CXSMICON, dpi != 0 ? dpi : 96);
}

void copyTruncated(wchar_t* destination, size_t capacity, const std::string& text) {
    const auto wide = core::toWide(text);
    wcsncpy_s(destination, capacity, wide.c_str(), _TRUNCATE);
}

}  // namespace

TrayIcon::~TrayIcon() {
    remove();
    if (icon_ != nullptr) {
        DestroyIcon(icon_);
    }
}

bool TrayIcon::create(HWND owner, UINT callbackMessage) {
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = kIconId;
    data_.uCallbackMessage = callbackMessage;
    icon_ = createTrayIcon(state_, trayIconSize(owner));
    data_.hIcon = icon_;
    copyTruncated(data_.szTip, std::size(data_.szTip), "PerfPulse");
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) {
        return false;
    }
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    added_ = true;
    return true;
}

void TrayIcon::recreate() {
    added_ = false;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    if (Shell_NotifyIconW(NIM_ADD, &data_)) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
        added_ = true;
    }
}

void TrayIcon::remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::setState(TrayState state) {
    if (state == state_ && icon_ != nullptr) {
        return;
    }
    state_ = state;
    HICON previous = icon_;
    icon_ = createTrayIcon(state, trayIconSize(data_.hWnd));
    data_.hIcon = icon_;
    apply(NIM_MODIFY, NIF_ICON);
    if (previous != nullptr) {
        DestroyIcon(previous);
    }
}

void TrayIcon::setTooltip(const std::string& text) {
    copyTruncated(data_.szTip, std::size(data_.szTip), text);
    apply(NIM_MODIFY, NIF_TIP | NIF_SHOWTIP);
}

void TrayIcon::showBalloon(const std::string& title, const std::string& text, bool quiet) {
    copyTruncated(data_.szInfoTitle, std::size(data_.szInfoTitle), title);
    copyTruncated(data_.szInfo, std::size(data_.szInfo), text);
    data_.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON | (quiet ? NIIF_NOSOUND | NIIF_RESPECT_QUIET_TIME : 0);
    data_.hBalloonIcon = icon_;
    apply(NIM_MODIFY, NIF_INFO);
    data_.szInfo[0] = L'\0';
}

void TrayIcon::apply(DWORD message, UINT flags) {
    if (!added_) {
        return;
    }
    data_.uFlags = flags;
    Shell_NotifyIconW(message, &data_);
}

}  // namespace perfpulse::app
