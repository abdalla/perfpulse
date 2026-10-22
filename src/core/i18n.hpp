#pragma once

#include <string>
#include <string_view>

#include "core/config.hpp"

namespace perfpulse::core {

// Resolves Language::Auto from the Windows UI language.
Language resolveLanguage(Language language);

// BCP-47 tag for the resolved language ("pt-BR" or "en").
std::string_view languageTag(Language language);

// Sets the language used by tr(). Thread-safe to read afterwards.
void setLanguage(Language language);
Language currentLanguage();

// Looks up a UI string. Unknown keys are returned verbatim so that missing
// translations are visible instead of crashing.
std::string_view tr(std::string_view key);

// Replaces "{0}", "{1}"... with the given arguments.
std::string format(std::string_view pattern, std::initializer_list<std::string_view> args);

}  // namespace perfpulse::core
