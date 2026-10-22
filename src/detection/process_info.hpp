#pragma once

#include <windows.h>

#include <string>

namespace perfpulse::detection {

struct ProcessInfo {
    DWORD processId = 0;
    std::string exePath;
    std::string exeName;      // lower case, e.g. "cyberpunk2077.exe"
    std::string productName;  // from the version resource
    std::string fileDescription;
};

// Returns an empty exePath when the process cannot be queried.
ProcessInfo queryProcess(DWORD processId);

// Picks the most human name for a game: version resource, then window title,
// then the executable stem. Skips generic engine names ("Unreal Engine"...).
std::string resolveGameName(const ProcessInfo& process, const std::string& windowTitle);

std::string windowTitle(HWND window);

}  // namespace perfpulse::detection
