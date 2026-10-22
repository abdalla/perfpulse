#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace perfpulse::detection {

struct MonitorInfo {
    HMONITOR handle = nullptr;
    std::string deviceName;    // "\\.\DISPLAY1", stable across sessions
    std::string friendlyName;  // "LG ULTRAGEAR" when EDID data is available
    RECT bounds{};
    int refreshHz = 0;
    bool primary = false;

    int width() const { return bounds.right - bounds.left; }
    int height() const { return bounds.bottom - bounds.top; }
    // "1 · LG ULTRAGEAR (2560×1440 @ 165 Hz)"
    std::string displayLabel(size_t index) const;
};

std::vector<MonitorInfo> enumerateMonitors();
MonitorInfo describeMonitor(HMONITOR monitor);

}  // namespace perfpulse::detection
