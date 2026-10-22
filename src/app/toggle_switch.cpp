#include "app/toggle_switch.hpp"

#include <windowsx.h>

#include <memory>

#include "app/gdiplus_include.hpp"
#include "app/theme.hpp"

namespace perfpulse::app {

namespace {

constexpr wchar_t kClassName[] = L"PerfPulseToggle";
constexpr int kSwitchWidth = 36;
constexpr int kSwitchHeight = 20;
constexpr int kGap = 10;
constexpr int kTrailing = 18;
constexpr int kHeight = 26;

struct ToggleState {
    std::wstring label;
    HFONT font = nullptr;
    double scale = 1.0;
    bool checked = false;
    bool hover = false;
    bool focused = false;
};

ToggleState* stateOf(HWND window) {
    return reinterpret_cast<ToggleState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

Gdiplus::Color toGdiplus(COLORREF color) {
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void fillRoundRect(Gdiplus::Graphics& g, const Gdiplus::Brush& brush, Gdiplus::RectF r, float radius) {
    Gdiplus::GraphicsPath path;
    const float d = radius * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

void paint(HWND window, HDC target) {
    const auto* state = stateOf(window);
    const auto& colors = palette();
    RECT client;
    GetClientRect(window, &client);

    // Double buffer to avoid flicker while toggling quickly.
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    FillRect(dc, &client, brush(colors.surface));

    const float s = static_cast<float>(state->scale);
    const float w = kSwitchWidth * s;
    const float h = kSwitchHeight * s;
    const float top = (static_cast<float>(client.bottom) - h) / 2.0f;
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const Gdiplus::RectF track(0.5f, top, w, h);
        const COLORREF trackColor = state->checked ? colors.toggleOn : colors.toggleOff;
        Gdiplus::SolidBrush trackBrush(toGdiplus(trackColor));
        fillRoundRect(g, trackBrush, track, h / 2.0f);
        if (!state->checked) {
            Gdiplus::Pen outline(toGdiplus(state->hover ? colors.mutedText : colors.border), 1.2f * s);
            Gdiplus::GraphicsPath path;
            const float d = h;
            path.AddArc(track.X, track.Y, d, d, 90, 180);
            path.AddArc(track.X + track.Width - d, track.Y, d, d, 270, 180);
            path.CloseFigure();
            g.DrawPath(&outline, &path);
        }
        const float knob = h - 6.0f * s;
        const float knobX = state->checked ? track.X + w - knob - 3.0f * s : track.X + 3.0f * s;
        const COLORREF knobColor = state->checked ? colors.knob : RGB(70, 70, 80);
        Gdiplus::SolidBrush knobBrush(toGdiplus(knobColor));
        g.FillEllipse(&knobBrush, knobX, top + 3.0f * s, knob, knob);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.text);
    HGDIOBJ oldFont = SelectObject(dc, state->font);
    RECT textRect = client;
    textRect.left = static_cast<LONG>(w + kGap * s);
    DrawTextW(dc, state->label.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    if (state->focused) {
        RECT focus = textRect;
        DrawTextW(dc, state->label.c_str(), -1, &focus, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
        focus.top = (client.bottom - (focus.bottom - focus.top)) / 2 - 1;
        focus.bottom = client.bottom - focus.top;
        InflateRect(&focus, 3, 0);
        DrawFocusRect(dc, &focus);
    }
    SelectObject(dc, oldFont);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void toggle(HWND window) {
    auto* state = stateOf(window);
    state->checked = !state->checked;
    InvalidateRect(window, nullptr, FALSE);
    SendMessageW(GetParent(window), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(window), BN_CLICKED),
                 reinterpret_cast<LPARAM>(window));
}

}  // namespace

void ToggleSwitch::registerClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &ToggleSwitch::windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);
}

HWND ToggleSwitch::create(HWND parent, int id, const std::wstring& label, int x, int y, HFONT font, double scale) {
    auto state = std::make_unique<ToggleState>();
    state->label = label;
    state->font = font;
    state->scale = scale;
    const int width = measure(parent, label, font, scale);
    HWND window = CreateWindowExW(0, kClassName, label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, y, width,
                                  static_cast<int>(kHeight * scale), parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
    if (window != nullptr) {
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state.release()));
    }
    return window;
}

int ToggleSwitch::measure(HWND parent, const std::wstring& label, HFONT font, double scale) {
    HDC dc = GetDC(parent);
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(parent, dc);
    return static_cast<int>((kSwitchWidth + kGap + kTrailing) * scale) + size.cx;
}

bool ToggleSwitch::checked(HWND toggle) {
    return SendMessageW(toggle, kGetCheck, 0, 0) != 0;
}

void ToggleSwitch::setChecked(HWND toggle, bool checked) {
    SendMessageW(toggle, kSetCheck, checked ? TRUE : FALSE, 0);
}

LRESULT CALLBACK ToggleSwitch::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = stateOf(window);
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(window, &ps);
            if (state != nullptr) {
                paint(window, dc);
            }
            EndPaint(window, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN:
            SetFocus(window);
            SetCapture(window);
            return 0;
        case WM_LBUTTONUP: {
            if (GetCapture() == window) {
                ReleaseCapture();
                RECT client;
                GetClientRect(window, &client);
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (PtInRect(&client, point)) {
                    toggle(window);
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (state != nullptr && !state->hover) {
                state->hover = true;
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                TrackMouseEvent(&track);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            if (state != nullptr) {
                state->hover = false;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_KEYUP:
            if (wParam == VK_SPACE) {
                toggle(window);
            }
            return 0;
        case WM_GETDLGCODE:
            return DLGC_BUTTON | DLGC_WANTCHARS;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            if (state != nullptr) {
                state->focused = message == WM_SETFOCUS;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case kSetCheck:
            if (state != nullptr) {
                state->checked = wParam != 0;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case kGetCheck:
            return state != nullptr && state->checked ? 1 : 0;
        case WM_NCDESTROY:
            delete state;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace perfpulse::app
