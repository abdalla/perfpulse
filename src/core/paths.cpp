#include "core/paths.hpp"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>

#include "core/log.hpp"
#include "core/strings.hpp"

namespace perfpulse::core {

namespace {

std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw))) {
        result = raw;
    }
    CoTaskMemFree(raw);
    return result;
}

}  // namespace

std::filesystem::path configDirectory() {
    return knownFolder(FOLDERID_RoamingAppData) / L"PerfPulse";
}

std::filesystem::path configFilePath() {
    return configDirectory() / L"config.json";
}

std::filesystem::path localDataDirectory() {
    return knownFolder(FOLDERID_LocalAppData) / L"PerfPulse";
}

std::filesystem::path executablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path executableDirectory() {
    return executablePath().parent_path();
}

std::filesystem::path defaultReportDirectory() {
    return knownFolder(FOLDERID_Documents) / L"PerfPulse" / L"Reports";
}

std::filesystem::path reportDirectory(const Config& config) {
    if (config.reportDirectory.empty()) {
        return defaultReportDirectory();
    }
    return std::filesystem::path(toWide(config.reportDirectory));
}

Config loadConfig() {
    const auto text = readTextFile(configFilePath());
    if (!text) {
        return Config::defaults();
    }
    const auto json = Json::parse(*text);
    if (!json || !json->isObject()) {
        log::warn("config.json is invalid, using defaults");
        return Config::defaults();
    }
    return Config::fromJson(*json);
}

bool saveConfig(const Config& config) {
    return writeTextFileAtomic(configFilePath(), config.toJson().dump());
}

std::optional<std::string> readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool writeTextFileAtomic(const std::filesystem::path& path, std::string_view content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            log::error("Cannot write " + toUtf8(temporary.wstring()));
            return false;
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!stream) {
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        log::error("Cannot replace " + toUtf8(path.wstring()));
        return false;
    }
    return true;
}

}  // namespace perfpulse::core
