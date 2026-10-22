#include "core/log.hpp"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "core/paths.hpp"

namespace perfpulse::core::log {

namespace {

std::mutex gMutex;
std::ofstream gStream;

void write(std::string_view level, std::string_view message) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "%04u-%02u-%02u %02u:%02u:%02u.%03u [%.*s] ", now.wYear, now.wMonth,
                  now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                  static_cast<int>(level.size()), level.data());
    std::lock_guard lock(gMutex);
    if (!gStream.is_open()) {
        return;
    }
    gStream << prefix << message << '\n';
    gStream.flush();
}

}  // namespace

void initialize() {
    std::lock_guard lock(gMutex);
    const auto directory = localDataDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto path = directory / L"perfpulse.log";
    if (std::filesystem::exists(path, error) && std::filesystem::file_size(path, error) > 1024 * 1024) {
        std::filesystem::rename(path, directory / L"perfpulse.old.log", error);
    }
    gStream.open(path, std::ios::app);
}

void info(std::string_view message) {
    write("INFO", message);
}

void warn(std::string_view message) {
    write("WARN", message);
}

void error(std::string_view message) {
    write("ERROR", message);
}

}  // namespace perfpulse::core::log
