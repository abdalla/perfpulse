#pragma once

#include <string>
#include <string_view>

namespace perfpulse::core {

// PerfPulse keeps text as UTF-8 internally and converts at the Win32 boundary.
std::wstring toWide(std::string_view utf8);
std::string toUtf8(std::wstring_view wide);

std::string toLower(std::string_view text);
std::string trim(std::string_view text);

// Replaces characters that are invalid in Windows file names.
std::string sanitizeFileName(std::string_view text);

}  // namespace perfpulse::core
