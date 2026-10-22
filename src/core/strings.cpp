#include "core/strings.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>

namespace perfpulse::core {

std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

std::string toUtf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr,
                        nullptr);
    return result;
}

std::string toLower(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string trim(std::string_view text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && isSpace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && isSpace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string sanitizeFileName(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char c : text) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) {
            continue;
        }
        result.push_back(c == ' ' ? '_' : c);
    }
    if (result.empty()) {
        result = "game";
    }
    if (result.size() > 60) {
        result.resize(60);
    }
    return result;
}

}  // namespace perfpulse::core
