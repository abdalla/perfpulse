#include "report/resource_loader.hpp"

#include <windows.h>

namespace perfpulse::report {

std::string_view loadTextResource(int resourceId) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC info = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (info == nullptr) {
        return {};
    }
    HGLOBAL handle = LoadResource(module, info);
    if (handle == nullptr) {
        return {};
    }
    const auto* data = static_cast<const char*>(LockResource(handle));
    return data != nullptr ? std::string_view(data, SizeofResource(module, info)) : std::string_view();
}

}  // namespace perfpulse::report
