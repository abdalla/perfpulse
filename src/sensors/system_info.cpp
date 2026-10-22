#include "sensors/system_info.hpp"

#include <windows.h>

#include <set>
#include <vector>

#include "core/strings.hpp"
#include "core/win_handle.hpp"

namespace perfpulse::sensors {

namespace {

std::wstring readRegistryString(HKEY root, const wchar_t* key, const wchar_t* value) {
    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
        return {};
    }
    return buffer;
}

DWORD readRegistryDword(HKEY root, const wchar_t* key, const wchar_t* value) {
    DWORD data = 0;
    DWORD size = sizeof(data);
    RegGetValueW(root, key, value, RRF_RT_REG_DWORD, nullptr, &data, &size);
    return data;
}

void queryCoreTopology(session::HardwareInfo& info) {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    std::vector<BYTE> buffer(length);
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()),
            &length)) {
        return;
    }
    // Hybrid CPUs (Intel 12th gen+) report a higher EfficiencyClass for P-cores.
    std::vector<BYTE> classes;
    for (DWORD offset = 0; offset < length;) {
        const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        classes.push_back(entry->Processor.EfficiencyClass);
        for (WORD g = 0; g < entry->Processor.GroupCount; ++g) {
            KAFFINITY mask = entry->Processor.GroupMask[g].Mask;
            while (mask != 0) {
                info.cpuLogicalProcessors += static_cast<int>(mask & 1);
                mask >>= 1;
            }
        }
        offset += entry->Size;
    }
    BYTE highest = 0;
    for (const BYTE c : classes) {
        highest = std::max(highest, c);
    }
    const bool hybrid = std::set<BYTE>(classes.begin(), classes.end()).size() > 1;
    for (const BYTE c : classes) {
        (hybrid && c != highest ? info.cpuEfficiencyCores : info.cpuPerformanceCores)++;
    }
}

}  // namespace

session::HardwareInfo querySystemInfo() {
    session::HardwareInfo info;
    info.cpuName = core::trim(core::toUtf8(readRegistryString(
        HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString")));
    queryCoreTopology(info);

    ULONGLONG installedKb = 0;
    if (GetPhysicallyInstalledSystemMemory(&installedKb)) {
        info.ramTotalGb = static_cast<float>(static_cast<double>(installedKb) / (1024.0 * 1024.0));
    }

    constexpr auto kVersionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const auto product = core::toUtf8(readRegistryString(HKEY_LOCAL_MACHINE, kVersionKey, L"ProductName"));
    const auto display = core::toUtf8(readRegistryString(HKEY_LOCAL_MACHINE, kVersionKey, L"DisplayVersion"));
    const auto build = core::toUtf8(readRegistryString(HKEY_LOCAL_MACHINE, kVersionKey, L"CurrentBuild"));
    const DWORD ubr = readRegistryDword(HKEY_LOCAL_MACHINE, kVersionKey, L"UBR");
    auto name = product;
    // ProductName still says "Windows 10" on Windows 11; the build number tells them apart.
    if (!build.empty() && std::stoi(build) >= 22000 && name.find("Windows 10") != std::string::npos) {
        name.replace(name.find("Windows 10"), 10, "Windows 11");
    }
    info.osVersion = name + (display.empty() ? "" : " " + display) + " (build " + build + "." + std::to_string(ubr) +
                     ")";
    return info;
}

bool isProcessElevated() {
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) {
        return false;
    }
    core::UniqueHandle token(rawToken);
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    return GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size) &&
           elevation.TokenIsElevated != 0;
}

}  // namespace perfpulse::sensors
