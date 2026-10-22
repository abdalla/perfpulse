#include "detection/process_info.hpp"

#include <array>
#include <filesystem>
#include <vector>

#include "core/strings.hpp"
#include "core/win_handle.hpp"

namespace perfpulse::detection {

namespace {

std::string queryVersionString(const std::vector<BYTE>& block, const wchar_t* field) {
    struct Translation {
        WORD language;
        WORD codePage;
    };
    Translation* translations = nullptr;
    UINT size = 0;
    std::vector<std::wstring> candidates;
    if (VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations),
                       &size)) {
        for (UINT i = 0; i < size / sizeof(Translation); ++i) {
            wchar_t key[64];
            swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\%s", translations[i].language, translations[i].codePage,
                       field);
            candidates.emplace_back(key);
        }
    }
    // Common fallbacks: US English with Unicode or Western code page.
    for (const auto* fallback : {L"040904b0", L"040904e4", L"000004b0"}) {
        candidates.push_back(std::wstring(L"\\StringFileInfo\\") + fallback + L"\\" + field);
    }
    for (const auto& key : candidates) {
        wchar_t* value = nullptr;
        UINT length = 0;
        if (VerQueryValueW(block.data(), key.c_str(), reinterpret_cast<void**>(&value), &length) && length > 1) {
            return core::trim(core::toUtf8(value));
        }
    }
    return {};
}

bool isGenericName(const std::string& name) {
    static constexpr std::array<std::string_view, 16> kGeneric = {
        "unreal engine", "unrealengine", "unity", "unity player", "bootstrappackagedgame", "launcher",
        "game", "client", "application", "directx", "vulkan", "opengl", "win64", "shipping", "main", "app",
    };
    const auto lower = core::toLower(name);
    if (lower.empty() || lower.size() < 2) {
        return true;
    }
    for (const auto generic : kGeneric) {
        if (lower == generic) {
            return true;
        }
    }
    return lower.find("unreal engine") != std::string::npos || lower.find("unity player") != std::string::npos;
}

}  // namespace

ProcessInfo queryProcess(DWORD processId) {
    ProcessInfo info;
    info.processId = processId;
    core::UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
    if (!process) {
        return info;
    }
    std::wstring path(MAX_PATH * 4, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &length)) {
        return info;
    }
    path.resize(length);
    info.exePath = core::toUtf8(path);
    info.exeName = core::toLower(core::toUtf8(std::filesystem::path(path).filename().wstring()));

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size > 0) {
        std::vector<BYTE> block(size);
        if (GetFileVersionInfoW(path.c_str(), 0, size, block.data())) {
            info.productName = queryVersionString(block, L"ProductName");
            info.fileDescription = queryVersionString(block, L"FileDescription");
        }
    }
    return info;
}

std::string resolveGameName(const ProcessInfo& process, const std::string& title) {
    for (const auto& candidate : {process.productName, process.fileDescription, core::trim(title)}) {
        if (!isGenericName(candidate) && candidate.size() <= 80) {
            return candidate;
        }
    }
    auto stem = core::toUtf8(std::filesystem::path(core::toWide(process.exeName)).stem().wstring());
    // "FortniteClient-Win64-Shipping" -> "FortniteClient"
    for (const auto suffix : {"-win64-shipping", "-wingdk-shipping", "_x64", "-win64", "64"}) {
        const auto lower = core::toLower(stem);
        const std::string_view s(suffix);
        if (lower.size() > s.size() && lower.compare(lower.size() - s.size(), s.size(), s) == 0) {
            stem.resize(stem.size() - s.size());
        }
    }
    return stem.empty() ? process.exeName : stem;
}

std::string windowTitle(HWND window) {
    wchar_t buffer[256];
    const int length = GetWindowTextW(window, buffer, static_cast<int>(std::size(buffer)));
    return length > 0 ? core::toUtf8(std::wstring_view(buffer, static_cast<size_t>(length))) : std::string();
}

}  // namespace perfpulse::detection
