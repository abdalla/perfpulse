#pragma once

#include <windows.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "detection/monitor_info.hpp"

namespace perfpulse::app {

// The settings window: "What to show" toggles (with presets), monitor
// selection, detection and report options. Changes are applied only on Save.
class SettingsWindow {
public:
    using SaveHandler = std::function<void(const core::Config&)>;

    SettingsWindow(HINSTANCE instance, core::Config config, std::string sourcesSummary, SaveHandler onSave);
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    static void registerClass(HINSTANCE instance);

    void show();
    bool isOpen() const { return window_ != nullptr; }
    // Updates the "Detected sources" footer (the probe may finish after the window opened).
    void setSourcesSummary(std::string summary);
    HWND handle() const { return window_; }

private:
    struct Label {
        RECT rect;
        std::wstring text;
        bool heading;
        bool wrap;
    };
    struct Card {
        RECT rect;
    };

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createFonts();
    void destroyFonts();
    void buildLayout();
    void paint(HDC dc);
    void drawButton(const DRAWITEMSTRUCT& item);
    void applyPreset(const core::MetricSet& preset);
    void browseForFolder();
    void save();

    int scaled(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    HWND createControl(const wchar_t* className, const std::wstring& text, DWORD style, int x, int y, int w, int h,
                       int id, DWORD exStyle = 0);
    void addLabel(int x, int y, int w, int h, const std::string& text, bool heading = false, bool wrap = false);

    HINSTANCE instance_;
    HWND window_ = nullptr;
    UINT dpi_ = 96;
    core::Config config_;
    std::string sourcesSummary_;
    SaveHandler onSave_;
    std::vector<detection::MonitorInfo> monitors_;

    HFONT font_ = nullptr;
    HFONT headingFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    std::vector<Label> labels_;
    std::vector<Card> cards_;
    std::map<int, core::Metric> metricToggles_;  // control id -> metric
    std::vector<HWND> ownerDrawnButtons_;

    HWND monitorCombo_ = nullptr;
    HWND fullscreenToggle_ = nullptr;
    HWND delayEdit_ = nullptr;
    HWND minSessionEdit_ = nullptr;
    HWND intervalCombo_ = nullptr;
    HWND folderEdit_ = nullptr;
    HWND finishCombo_ = nullptr;
    HWND languageCombo_ = nullptr;
    HWND autostartToggle_ = nullptr;
    HWND alwaysEdit_ = nullptr;
    HWND neverEdit_ = nullptr;
    RECT sourcesRect_{};
};

}  // namespace perfpulse::app
