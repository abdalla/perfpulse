#include "detection/monitor_info.hpp"

#include <map>

#include "core/strings.hpp"

namespace perfpulse::detection {

namespace {

// Maps GDI device names to monitor names read from EDID via the DisplayConfig API.
std::map<std::wstring, std::wstring> friendlyNamesByDevice() {
    std::map<std::wstring, std::wstring> names;
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        return names;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) !=
        ERROR_SUCCESS) {
        return names;
    }
    for (UINT32 i = 0; i < pathCount; ++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = paths[i].sourceInfo.adapterId;
        source.header.id = paths[i].sourceInfo.id;
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size = sizeof(target);
        target.header.adapterId = paths[i].targetInfo.adapterId;
        target.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS &&
            DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS && target.monitorFriendlyDeviceName[0]) {
            names[source.viewGdiDeviceName] = target.monitorFriendlyDeviceName;
        }
    }
    return names;
}

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
    reinterpret_cast<std::vector<HMONITOR>*>(context)->push_back(monitor);
    return TRUE;
}

MonitorInfo describe(HMONITOR monitor, const std::map<std::wstring, std::wstring>& friendlyNames) {
    MonitorInfo info;
    info.handle = monitor;
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return info;
    }
    info.deviceName = core::toUtf8(monitorInfo.szDevice);
    info.bounds = monitorInfo.rcMonitor;
    info.primary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        info.refreshHz = static_cast<int>(mode.dmDisplayFrequency);
    }
    const auto it = friendlyNames.find(monitorInfo.szDevice);
    info.friendlyName = it != friendlyNames.end() ? core::toUtf8(it->second) : info.deviceName;
    return info;
}

}  // namespace

std::string MonitorInfo::displayLabel(size_t index) const {
    return std::to_string(index + 1) + " \xC2\xB7 " + friendlyName + " (" + std::to_string(width()) +
           "\xC3\x97" + std::to_string(height()) + " @ " + std::to_string(refreshHz) + " Hz)" +
           (primary ? " \xE2\x98\x85" : "");
}

std::vector<MonitorInfo> enumerateMonitors() {
    std::vector<HMONITOR> handles;
    EnumDisplayMonitors(nullptr, nullptr, &collectMonitor, reinterpret_cast<LPARAM>(&handles));
    const auto names = friendlyNamesByDevice();
    std::vector<MonitorInfo> monitors;
    for (const auto handle : handles) {
        monitors.push_back(describe(handle, names));
    }
    return monitors;
}

MonitorInfo describeMonitor(HMONITOR monitor) {
    return describe(monitor, friendlyNamesByDevice());
}

}  // namespace perfpulse::detection
