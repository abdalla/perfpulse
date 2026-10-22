#pragma once

#include <string_view>

namespace perfpulse::report {

// Text of an RCDATA resource embedded in the executable (report templates).
// The memory belongs to the loaded image and stays valid for the process lifetime.
std::string_view loadTextResource(int resourceId);

}  // namespace perfpulse::report
