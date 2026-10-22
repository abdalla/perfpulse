#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "core/config.hpp"

namespace perfpulse::core {

// %APPDATA%\PerfPulse
std::filesystem::path configDirectory();
std::filesystem::path configFilePath();
// %LOCALAPPDATA%\PerfPulse
std::filesystem::path localDataDirectory();
// Directory containing PerfPulse.exe.
std::filesystem::path executableDirectory();
std::filesystem::path executablePath();

std::filesystem::path defaultReportDirectory();
std::filesystem::path reportDirectory(const Config& config);

Config loadConfig();
bool saveConfig(const Config& config);

std::optional<std::string> readTextFile(const std::filesystem::path& path);
// Writes through a temporary file and renames it, so readers never see a partial file.
bool writeTextFileAtomic(const std::filesystem::path& path, std::string_view content);

}  // namespace perfpulse::core
