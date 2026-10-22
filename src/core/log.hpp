#pragma once

#include <string_view>

namespace perfpulse::core::log {

// Appends to %LOCALAPPDATA%\PerfPulse\perfpulse.log. The file is rotated at
// startup when it grows past 1 MB. Calls are thread-safe and cheap enough for
// state changes, but must not be used per frame.
void initialize();
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

}  // namespace perfpulse::core::log
