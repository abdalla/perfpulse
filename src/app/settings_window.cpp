#include "app/settings_window.hpp"

#include <commctrl.h>
#include <shobjidl.h>

#include <algorithm>
#include <sstream>

#include "app/gdiplus_include.hpp"
#include "app/theme.hpp"
#include "app/toggle_switch.hpp"
#include "core/i18n.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"

namespace perfpulse::app {

namespace {

constexpr wchar_t kClassName[] = L"PerfPulseSettings";

constexpr int kMetricToggleBase = 1000;
constexpr int kPresetFpsOnly = 2001;
constexpr int kPresetDefault = 2002;
constexpr int kPresetAll = 2003;
constexpr int kMonitorCombo = 2101;
constexpr int kFullscreenToggle = 2102;
constexpr int kDelayEdit = 2103;
constexpr int kMinSessionEdit = 2104;
constexpr int kIntervalCombo = 2105;
constexpr int kFolderEdit = 2106;
constexpr int kBrowseButton = 2107;
constexpr int kFinishCombo = 2108;
constexpr int kLanguageCombo = 2109;
constexpr int kAutostartToggle = 2110;
constexpr int kAlwaysEdit = 2111;
constexpr int kNeverEdit = 2112;

constexpr int kMargin = 16;
constexpr int kPadding = 18;
constexpr int kLeftWidth = 640;
constexpr int kRightWidth = 390;
constexpr int kToggleRow = 30;
constexpr int kInputHeight = 24;

std::wstring wtr(std::string_view key) {
    return core::toWide(core::tr(key));
}

std::wstring windowText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::vector<std::string> parseExeList(const std::wstring& text) {
    std::vector<std::string> result;
    std::string current;
    const auto flush = [&] {
        auto name = core::toLower(core::trim(current));
        current.clear();
        if (name.empty()) {
            return;
        }
        if (name.size() < 4 || name.substr(name.size() - 4) != ".exe") {
            name += ".exe";
        }
        if (std::find(result.begin(), result.end(), name) == result.end()) {
            result.push_back(std::move(name));
        }
    };
    for (const char c : core::toUtf8(text)) {
        if (c == '\r' || c == '\n' || c == ',' || c == ';') {
            flush();
        } else {
            current.push_back(c);
        }
    }
    flush();
    return result;
}

std::wstring joinExeList(const std::vector<std::string>& list) {
    std::string text;
    for (const auto& name : list) {
        text += name + "\r\n";
    }
    return core::toWide(text);
}

int readNumber(HWND edit, int fallback, int low, int high) {
    try {
        return std::clamp(std::stoi(windowText(edit)), low, high);
    } catch (...) {
        return fallback;
    }
}

Gdiplus::Color gdiColor(COLORREF color) {
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void roundedPath(Gdiplus::GraphicsPath& path, const RECT& r, float radius) {
    const auto x = static_cast<float>(r.left) + 0.5f;
    const auto y = static_cast<float>(r.top) + 0.5f;
    const auto w = static_cast<float>(r.right - r.left) - 1.0f;
    const auto h = static_cast<float>(r.bottom - r.top) - 1.0f;
    const float d = std::min(radius * 2.0f, std::min(w, h));
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

}  // namespace

SettingsWindow::SettingsWindow(HINSTANCE instance, core::Config config, std::string sourcesSummary,
                               SaveHandler onSave)
    : instance_(instance),
      config_(std::move(config)),
      sourcesSummary_(std::move(sourcesSummary)),
      onSave_(std::move(onSave)) {}

SettingsWindow::~SettingsWindow() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
    destroyFonts();
}

void SettingsWindow::registerClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &SettingsWindow::windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);
    ToggleSwitch::registerClass(instance);
}

void SettingsWindow::show() {
    if (window_ != nullptr) {
        ShowWindow(window_, SW_RESTORE);
        SetForegroundWindow(window_);
        return;
    }
    refreshTheme();
    window_ = CreateWindowExW(WS_EX_CONTROLPARENT, kClassName, wtr("settings.title").c_str(),
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) {
        return;
    }
    applyWindowTheme(window_);
    dpi_ = GetDpiForWindow(window_);
    createFonts();
    buildLayout();
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
}

void SettingsWindow::setSourcesSummary(std::string summary) {
    sourcesSummary_ = std::move(summary);
    if (window_ != nullptr) {
        InvalidateRect(window_, &sourcesRect_, FALSE);
    }
}

void SettingsWindow::createFonts() {
    destroyFonts();
    const auto make = [this](int points, int weight) {
        return CreateFontW(-MulDiv(points, static_cast<int>(dpi_), 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Segoe UI");
    };
    font_ = make(10, FW_NORMAL);
    headingFont_ = make(9, FW_SEMIBOLD);
    smallFont_ = make(8, FW_NORMAL);
}

void SettingsWindow::destroyFonts() {
    for (HFONT* font : {&font_, &headingFont_, &smallFont_}) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

HWND SettingsWindow::createControl(const wchar_t* className, const std::wstring& text, DWORD style, int x, int y,
                                   int w, int h, int id, DWORD exStyle) {
    HWND control = CreateWindowExW(exStyle, className, text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                                   window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    applyControlTheme(control);
    return control;
}

void SettingsWindow::addLabel(int x, int y, int w, int h, const std::string& text, bool heading, bool wrap) {
    labels_.push_back({{x, y, x + w, y + h}, core::toWide(text), heading, wrap});
}

void SettingsWindow::buildLayout() {
    using core::Metric;
    using core::MetricGroup;
    const auto s = [this](int v) { return scaled(v); };
    const double scale = static_cast<double>(dpi_) / 96.0;
    labels_.clear();
    cards_.clear();
    metricToggles_.clear();
    ownerDrawnButtons_.clear();
    monitors_ = detection::enumerateMonitors();

    HDC dc = GetDC(window_);
    const auto textWidth = [dc](HFONT font, const std::wstring& text) {
        HGDIOBJ old = SelectObject(dc, font);
        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        SelectObject(dc, old);
        return static_cast<int>(size.cx);
    };

    const int top = s(kMargin);
    const int leftX = s(kMargin);
    const int leftW = s(kLeftWidth);
    const int rightX = leftX + leftW + s(kMargin);
    const int rightW = s(kRightWidth);

    // ---- Left card: what to show -------------------------------------------------
    int x = leftX + s(kPadding);
    int y = top + s(kPadding);
    const int innerRight = leftX + leftW - s(kPadding);
    addLabel(x, y, s(300), s(22), std::string(core::tr("settings.what_to_show")), true);

    int presetX = innerRight;
    for (const auto& [id, key] : {std::pair{kPresetAll, "settings.preset.all"},
                                  std::pair{kPresetDefault, "settings.preset.default"},
                                  std::pair{kPresetFpsOnly, "settings.preset.fps_only"}}) {
        const auto text = wtr(key);
        const int w = textWidth(headingFont_, text) + s(26);
        presetX -= w;
        HWND button = createControl(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, presetX, y - s(3), w, s(26), id);
        ownerDrawnButtons_.push_back(button);
        presetX -= s(8);
    }
    y += s(34);

    const auto hint = wtr("settings.what_to_show.hint");
    RECT hintRect{x, y, innerRight, y + s(200)};
    HGDIOBJ oldFont = SelectObject(dc, smallFont_);
    DrawTextW(dc, hint.c_str(), -1, &hintRect, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    labels_.push_back({hintRect, hint, false, true});
    y = hintRect.bottom + s(14);

    for (int g = 0; g < static_cast<int>(MetricGroup::Count); ++g) {
        const auto group = static_cast<MetricGroup>(g);
        addLabel(x, y, s(200), s(18), std::string(core::tr("group." + std::string(core::groupKey(group)))), true);
        y += s(22);
        int cx = x;
        for (const auto& info : core::allMetrics()) {
            if (info.group != group) {
                continue;
            }
            const auto label = wtr("metric." + std::string(info.key));
            const int w = ToggleSwitch::measure(window_, label, font_, scale);
            if (cx + w > innerRight && cx != x) {
                cx = x;
                y += s(kToggleRow);
            }
            const int id = kMetricToggleBase + static_cast<int>(info.metric);
            HWND toggle = ToggleSwitch::create(window_, id, label, cx, y, font_, scale);
            ToggleSwitch::setChecked(toggle, config_.metrics.has(info.metric));
            metricToggles_[id] = info.metric;
            cx += w;
        }
        y += s(kToggleRow) + s(8);
    }
    const int leftBottom = y + s(kPadding) - s(8);

    // ---- Right card: general ------------------------------------------------------
    x = rightX + s(kPadding);
    y = top + s(kPadding);
    const int w = rightW - 2 * s(kPadding);
    addLabel(x, y, w, s(22), std::string(core::tr("settings.general")), true);
    y += s(34);

    const auto fieldLabel = [&](int fx, int fw, const char* key) {
        addLabel(fx, y, fw, s(18), std::string(core::tr(key)));
    };

    fieldLabel(x, w, "settings.monitor");
    y += s(20);
    monitorCombo_ = createControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, x, y, w, s(240),
                                  kMonitorCombo);
    SendMessageW(monitorCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr("settings.monitor.any").c_str()));
    int monitorSelection = 0;
    for (size_t i = 0; i < monitors_.size(); ++i) {
        SendMessageW(monitorCombo_, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(core::toWide(monitors_[i].displayLabel(i)).c_str()));
        if (monitors_[i].deviceName == config_.monitorDevice) {
            monitorSelection = static_cast<int>(i) + 1;
        }
    }
    SendMessageW(monitorCombo_, CB_SETCURSEL, static_cast<WPARAM>(monitorSelection), 0);
    y += s(36);

    fullscreenToggle_ =
        ToggleSwitch::create(window_, kFullscreenToggle, wtr("settings.require_fullscreen"), x, y, font_, scale);
    ToggleSwitch::setChecked(fullscreenToggle_, config_.requireFullscreen);
    y += s(36);

    const int half = (w - s(12)) / 2;
    fieldLabel(x, half, "settings.detection_delay");
    fieldLabel(x + half + s(12), half, "settings.min_session");
    y += s(20);
    delayEdit_ = createControl(WC_EDITW, std::to_wstring(config_.detectionDelaySec),
                               ES_NUMBER | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, x, y, half, s(kInputHeight),
                               kDelayEdit);
    minSessionEdit_ = createControl(WC_EDITW, std::to_wstring(config_.minSessionSec),
                                    ES_NUMBER | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, x + half + s(12), y, half,
                                    s(kInputHeight), kMinSessionEdit);
    y += s(kInputHeight) + s(12);

    fieldLabel(x, w, "settings.sample_interval");
    y += s(20);
    intervalCombo_ = createControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, x, y, w, s(200),
                                   kIntervalCombo);
    int intervalSelection = 2;
    for (size_t i = 0; i < std::size(core::kSampleIntervalChoicesMs); ++i) {
        const int ms = core::kSampleIntervalChoicesMs[i];
        std::wstring text = ms < 1000 ? std::to_wstring(ms) + L" ms" : std::to_wstring(ms / 1000) + L" s";
        if (ms == 1000) {
            text += core::currentLanguage() == core::Language::English ? L" (recommended)" : L" (recomendado)";
        }
        SendMessageW(intervalCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        if (ms == config_.sampleIntervalMs) {
            intervalSelection = static_cast<int>(i);
        }
    }
    SendMessageW(intervalCombo_, CB_SETCURSEL, static_cast<WPARAM>(intervalSelection), 0);
    y += s(36);

    fieldLabel(x, w, "settings.report_dir");
    y += s(20);
    const int browseW = textWidth(font_, wtr("settings.browse")) + s(26);
    folderEdit_ = createControl(WC_EDITW, core::reportDirectory(config_).wstring(),
                                WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w - browseW - s(8), s(kInputHeight),
                                kFolderEdit);
    ownerDrawnButtons_.push_back(createControl(L"BUTTON", wtr("settings.browse"), BS_OWNERDRAW | WS_TABSTOP,
                                               x + w - browseW, y, browseW, s(kInputHeight), kBrowseButton));
    y += s(kInputHeight) + s(12);

    fieldLabel(x, w, "settings.finish_action");
    y += s(20);
    finishCombo_ =
        createControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, x, y, w, s(200), kFinishCombo);
    for (const auto* key : {"settings.finish.notify", "settings.finish.open", "settings.finish.silent"}) {
        SendMessageW(finishCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr(key).c_str()));
    }
    SendMessageW(finishCombo_, CB_SETCURSEL, static_cast<WPARAM>(config_.finishAction), 0);
    y += s(36);

    fieldLabel(x, w, "settings.language");
    y += s(20);
    languageCombo_ =
        createControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, x, y, w, s(200), kLanguageCombo);
    SendMessageW(languageCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr("settings.language.auto").c_str()));
    SendMessageW(languageCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Português (Brasil)"));
    SendMessageW(languageCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(languageCombo_, CB_SETCURSEL, static_cast<WPARAM>(config_.language), 0);
    y += s(36);

    autostartToggle_ =
        ToggleSwitch::create(window_, kAutostartToggle, wtr("settings.start_with_windows"), x, y, font_, scale);
    ToggleSwitch::setChecked(autostartToggle_, config_.startWithWindows);
    y += s(36);

    fieldLabel(x, w, "settings.always_monitor");
    y += s(20);
    alwaysEdit_ = createControl(WC_EDITW, joinExeList(config_.alwaysMonitor),
                                WS_BORDER | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                x, y, w, s(54), kAlwaysEdit);
    y += s(54) + s(12);
    fieldLabel(x, w, "settings.never_monitor");
    y += s(20);
    neverEdit_ = createControl(WC_EDITW, joinExeList(config_.neverMonitor),
                               WS_BORDER | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                               x, y, w, s(54), kNeverEdit);
    y += s(54);
    const int rightBottom = y + s(kPadding);

    const int cardBottom = std::max(leftBottom, rightBottom);
    cards_.push_back({{leftX, top, leftX + leftW, cardBottom}});
    cards_.push_back({{rightX, top, rightX + rightW, cardBottom}});

    // ---- Footer: detected sources + buttons -------------------------------------------
    y = cardBottom + s(14);
    const int totalRight = rightX + rightW;
    const int buttonW = s(116);
    ownerDrawnButtons_.push_back(createControl(L"BUTTON", wtr("settings.save"), BS_OWNERDRAW | WS_TABSTOP,
                                               totalRight - buttonW, y, buttonW, s(32), IDOK));
    ownerDrawnButtons_.push_back(createControl(L"BUTTON", wtr("settings.cancel"), BS_OWNERDRAW | WS_TABSTOP,
                                               totalRight - 2 * buttonW - s(10), y, buttonW, s(32), IDCANCEL));
    sourcesRect_ = {leftX + s(4), y, totalRight - 2 * buttonW - s(24), y + s(32)};
    ReleaseDC(window_, dc);

    RECT frame{0, 0, totalRight + s(kMargin), y + s(32) + s(kMargin)};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE)), FALSE,
                             static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE)), dpi_);
    const int frameW = frame.right - frame.left;
    const int frameH = frame.bottom - frame.top;
    // Center on the monitor under the cursor.
    POINT cursor;
    GetCursorPos(&cursor);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const RECT& work = monitor.rcWork;
    SetWindowPos(window_, nullptr, work.left + (work.right - work.left - frameW) / 2,
                 work.top + std::max(0L, (work.bottom - work.top - frameH) / 2), frameW, frameH, SWP_NOZORDER);
    InvalidateRect(window_, nullptr, TRUE);
}

void SettingsWindow::paint(HDC target) {
    const auto& colors = palette();
    RECT client;
    GetClientRect(window_, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    FillRect(dc, &client, brush(colors.background));
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush fill(gdiColor(colors.surface));
        Gdiplus::Pen outline(gdiColor(colors.border), 1.0f);
        for (const auto& card : cards_) {
            Gdiplus::GraphicsPath path;
            roundedPath(path, card.rect, static_cast<float>(scaled(12)));
            g.FillPath(&fill, &path);
            g.DrawPath(&outline, &path);
        }
    }
    SetBkMode(dc, TRANSPARENT);
    for (const auto& label : labels_) {
        RECT rect = label.rect;
        SelectObject(dc, label.heading ? headingFont_ : (label.wrap ? smallFont_ : font_));
        SetTextColor(dc, label.heading ? colors.text : colors.mutedText);
        DrawTextW(dc, label.text.c_str(), -1, &rect,
                  (label.wrap ? DT_WORDBREAK : DT_SINGLELINE | DT_VCENTER) | DT_LEFT | DT_NOPREFIX);
    }
    SelectObject(dc, smallFont_);
    SetTextColor(dc, colors.mutedText);
    const auto sources = wtr("settings.sources") + L":  " + core::toWide(sourcesSummary_);
    RECT sourcesRect = sourcesRect_;
    DrawTextW(dc, sources.c_str(), -1, &sourcesRect, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void SettingsWindow::drawButton(const DRAWITEMSTRUCT& item) {
    const auto& colors = palette();
    const bool primary = item.CtlID == IDOK;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const RECT r = item.rcItem;

    HDC dc = item.hDC;
    FillRect(dc, &r, brush(primary || item.CtlID == IDCANCEL ? colors.background : colors.surface));
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath path;
        roundedPath(path, r, static_cast<float>(r.bottom - r.top) / 2.0f);
        if (primary) {
            Gdiplus::SolidBrush fill(gdiColor(pressed ? colors.toggleOn : colors.accent));
            g.FillPath(&fill, &path);
        } else {
            Gdiplus::SolidBrush fill(gdiColor(pressed ? colors.border : colors.inputBackground));
            g.FillPath(&fill, &path);
            Gdiplus::Pen pen(gdiColor(focused ? colors.accent : colors.text), focused ? 1.6f : 1.1f);
            g.DrawPath(&pen, &path);
        }
    }
    wchar_t text[64];
    GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, primary ? colors.accentText : colors.text);
    HGDIOBJ oldFont = SelectObject(dc, headingFont_);
    RECT textRect = r;
    DrawTextW(dc, text, -1, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

void SettingsWindow::applyPreset(const core::MetricSet& preset) {
    for (const auto& [id, metric] : metricToggles_) {
        ToggleSwitch::setChecked(GetDlgItem(window_, id), preset.has(metric));
    }
}

void SettingsWindow::browseForFolder() {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    IShellItem* current = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(windowText(folderEdit_).c_str(), nullptr, IID_PPV_ARGS(&current)))) {
        dialog->SetFolder(current);
        current->Release();
    }
    if (SUCCEEDED(dialog->Show(window_))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                SetWindowTextW(folderEdit_, path);
                CoTaskMemFree(path);
            }
            result->Release();
        }
    }
    dialog->Release();
}

void SettingsWindow::save() {
    core::Config config = config_;
    for (const auto& [id, metric] : metricToggles_) {
        config.metrics.set(metric, ToggleSwitch::checked(GetDlgItem(window_, id)));
    }
    const auto monitorIndex = static_cast<int>(SendMessageW(monitorCombo_, CB_GETCURSEL, 0, 0));
    config.monitorDevice =
        monitorIndex > 0 && static_cast<size_t>(monitorIndex) <= monitors_.size()
            ? monitors_[static_cast<size_t>(monitorIndex) - 1].deviceName
            : std::string();
    config.requireFullscreen = ToggleSwitch::checked(fullscreenToggle_);
    config.detectionDelaySec = readNumber(delayEdit_, config.detectionDelaySec, 3, 300);
    config.minSessionSec = readNumber(minSessionEdit_, config.minSessionSec, 5, 3600);
    const auto interval = static_cast<int>(SendMessageW(intervalCombo_, CB_GETCURSEL, 0, 0));
    if (interval >= 0 && static_cast<size_t>(interval) < std::size(core::kSampleIntervalChoicesMs)) {
        config.sampleIntervalMs = core::kSampleIntervalChoicesMs[interval];
    }
    const auto folder = core::trim(core::toUtf8(windowText(folderEdit_)));
    config.reportDirectory =
        folder.empty() || std::filesystem::path(core::toWide(folder)) == core::defaultReportDirectory() ? ""
                                                                                                         : folder;
    config.finishAction =
        static_cast<core::FinishAction>(std::max<LRESULT>(0, SendMessageW(finishCombo_, CB_GETCURSEL, 0, 0)));
    config.language =
        static_cast<core::Language>(std::max<LRESULT>(0, SendMessageW(languageCombo_, CB_GETCURSEL, 0, 0)));
    config.startWithWindows = ToggleSwitch::checked(autostartToggle_);
    config.alwaysMonitor = parseExeList(windowText(alwaysEdit_));
    config.neverMonitor = parseExeList(windowText(neverEdit_));

    config_ = config;
    if (onSave_) {
        onSave_(config);
    }
    DestroyWindow(window_);
}

LRESULT CALLBACK SettingsWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    } else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    if (self == nullptr) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return self->handleMessage(message, wParam, lParam);
}

LRESULT SettingsWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const auto& colors = palette();
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(window_, &ps);
            paint(dc);
            EndPaint(window_, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, colors.text);
            SetBkColor(dc, colors.inputBackground);
            return reinterpret_cast<LRESULT>(brush(colors.inputBackground));
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, colors.text);
            SetBkColor(dc, colors.surface);
            return reinterpret_cast<LRESULT>(brush(colors.surface));
        }
        case WM_DRAWITEM:
            drawButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case kPresetFpsOnly: applyPreset(core::MetricSet::fpsOnly()); return 0;
                case kPresetDefault: applyPreset(core::MetricSet::defaults()); return 0;
                case kPresetAll: applyPreset(core::MetricSet::everything()); return 0;
                case kBrowseButton: browseForFolder(); return 0;
                case IDOK: save(); return 0;
                case IDCANCEL: DestroyWindow(window_); return 0;
            }
            break;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wParam);
            // Rebuild at the new DPI, keeping the user's unsaved edits.
            for (const auto& [id, metric] : metricToggles_) {
                config_.metrics.set(metric, ToggleSwitch::checked(GetDlgItem(window_, id)));
            }
            HWND child = GetWindow(window_, GW_CHILD);
            while (child != nullptr) {
                HWND next = GetWindow(child, GW_HWNDNEXT);
                DestroyWindow(child);
                child = next;
            }
            createFonts();
            buildLayout();
            return 0;
        }
        case WM_SETTINGCHANGE:
            if (lParam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet") {
                refreshTheme();
                applyWindowTheme(window_);
                InvalidateRect(window_, nullptr, TRUE);
                RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window_);
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
            window_ = nullptr;
            return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

}  // namespace perfpulse::app
