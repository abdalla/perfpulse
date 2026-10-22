#include "app/autostart.hpp"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/win_handle.hpp"

namespace perfpulse::app {

namespace {

constexpr wchar_t kTaskName[] = L"PerfPulse";

std::wstring currentUser() {
    wchar_t name[256];
    DWORD size = static_cast<DWORD>(std::size(name));
    std::wstring user = GetUserNameW(name, &size) ? name : L"";
    wchar_t domain[256];
    DWORD domainSize = static_cast<DWORD>(std::size(domain));
    if (GetEnvironmentVariableW(L"USERDOMAIN", domain, domainSize) > 0) {
        user = std::wstring(domain) + L"\\" + user;
    }
    return user;
}

std::wstring escapeXml(const std::wstring& text) {
    std::wstring result;
    for (const wchar_t c : text) {
        switch (c) {
            case L'&': result += L"&amp;"; break;
            case L'<': result += L"&lt;"; break;
            case L'>': result += L"&gt;"; break;
            case L'"': result += L"&quot;"; break;
            default: result.push_back(c);
        }
    }
    return result;
}

// Runs schtasks.exe hidden and returns its exit code.
DWORD runSchtasks(const std::wstring& arguments) {
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(system) + L"\\schtasks.exe\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process)) {
        return GetLastError();
    }
    core::UniqueHandle processHandle(process.hProcess);
    core::UniqueHandle threadHandle(process.hThread);
    WaitForSingleObject(process.hProcess, 15000);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    return exitCode;
}

std::wstring taskXml() {
    const auto exe = escapeXml(core::executablePath().wstring());
    const auto user = escapeXml(currentUser());
    // Defaults created by "schtasks /Create" would stop the task after 72 h and
    // on battery; the XML definition avoids both.
    return LR"(<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo><Description>PerfPulse game performance recorder</Description></RegistrationInfo>
  <Triggers>
    <LogonTrigger><Enabled>true</Enabled><UserId>)" +
           user + LR"(</UserId><Delay>PT15S</Delay></LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author"><UserId>)" +
           user + LR"(</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
    <AllowHardTerminate>true</AllowHardTerminate>
  </Settings>
  <Actions Context="Author">
    <Exec><Command>")" +
           exe + LR"("</Command><Arguments>--autostart</Arguments></Exec>
  </Actions>
</Task>
)";
}

}  // namespace

bool setAutostart(bool enabled) {
    if (!enabled) {
        const DWORD result = runSchtasks(std::wstring(L"/Delete /F /TN \"") + kTaskName + L"\"");
        return result == 0 || !isAutostartEnabled();
    }
    const auto xmlPath = core::localDataDirectory() / L"autostart-task.xml";
    std::error_code error;
    std::filesystem::create_directories(xmlPath.parent_path(), error);
    {
        std::ofstream stream(xmlPath, std::ios::binary | std::ios::trunc);
        const auto xml = taskXml();
        const wchar_t bom = 0xFEFF;
        stream.write(reinterpret_cast<const char*>(&bom), sizeof(bom));
        stream.write(reinterpret_cast<const char*>(xml.data()), static_cast<std::streamsize>(xml.size() * 2));
    }
    const DWORD result =
        runSchtasks(std::wstring(L"/Create /F /TN \"") + kTaskName + L"\" /XML \"" + xmlPath.wstring() + L"\"");
    std::filesystem::remove(xmlPath, error);
    if (result != 0) {
        core::log::error("schtasks /Create failed with exit code " + std::to_string(result));
    }
    return result == 0;
}

bool isAutostartEnabled() {
    return runSchtasks(std::wstring(L"/Query /TN \"") + kTaskName + L"\"") == 0;
}

}  // namespace perfpulse::app
