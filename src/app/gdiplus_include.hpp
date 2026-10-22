#pragma once

// GDI+ headers rely on the min/max macros that NOMINMAX removes.
#include <windows.h>

#include <algorithm>

namespace Gdiplus {
using std::max;
using std::min;
}  // namespace Gdiplus

#include <objidl.h>
#include <gdiplus.h>
