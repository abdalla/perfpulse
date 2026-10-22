#include "core/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace perfpulse::core {

namespace {

const Json::Array kEmptyArray;
const Json::Object kEmptyObject;

void appendEscaped(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            // Keeps "</script>" and HTML comments from terminating an embedding script block.
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            case '&': out += "\\u0026"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
                    out += buffer;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Json> parseDocument() {
        auto value = parseValue();
        skipWhitespace();
        if (!value || pos_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

private:
    std::optional<Json> parseValue() {
        skipWhitespace();
        if (pos_ >= text_.size() || ++depth_ > kMaxDepth) {
            return std::nullopt;
        }
        std::optional<Json> result;
        switch (text_[pos_]) {
            case '{': result = parseObject(); break;
            case '[': result = parseArray(); break;
            case '"': {
                auto text = parseString();
                if (text) {
                    result = Json(std::move(*text));
                }
                break;
            }
            case 't': result = parseLiteral("true", Json(true)); break;
            case 'f': result = parseLiteral("false", Json(false)); break;
            case 'n': result = parseLiteral("null", Json(nullptr)); break;
            default: result = parseNumber();
        }
        --depth_;
        return result;
    }

    std::optional<Json> parseObject() {
        ++pos_;
        Json::Object object;
        skipWhitespace();
        if (consume('}')) {
            return Json(std::move(object));
        }
        while (true) {
            skipWhitespace();
            auto key = parseString();
            if (!key) {
                return std::nullopt;
            }
            skipWhitespace();
            if (!consume(':')) {
                return std::nullopt;
            }
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            object.insert_or_assign(std::move(*key), std::move(*value));
            skipWhitespace();
            if (consume('}')) {
                return Json(std::move(object));
            }
            if (!consume(',')) {
                return std::nullopt;
            }
        }
    }

    std::optional<Json> parseArray() {
        ++pos_;
        Json::Array array;
        skipWhitespace();
        if (consume(']')) {
            return Json(std::move(array));
        }
        while (true) {
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            array.push_back(std::move(*value));
            skipWhitespace();
            if (consume(']')) {
                return Json(std::move(array));
            }
            if (!consume(',')) {
                return std::nullopt;
            }
        }
    }

    std::optional<std::string> parseString() {
        if (!consume('"')) {
            return std::nullopt;
        }
        std::string result;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return std::nullopt;
            }
            const char escape = text_[pos_++];
            switch (escape) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    unsigned codePoint = 0;
                    if (!parseHex4(codePoint)) {
                        return std::nullopt;
                    }
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        unsigned low = 0;
                        if (!consume('\\') || !consume('u') || !parseHex4(low)) {
                            return std::nullopt;
                        }
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(result, codePoint);
                    break;
                }
                default: return std::nullopt;
            }
        }
        return std::nullopt;
    }

    bool parseHex4(unsigned& value) {
        if (pos_ + 4 > text_.size()) {
            return false;
        }
        const auto [ptr, ec] = std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, value, 16);
        if (ec != std::errc() || ptr != text_.data() + pos_ + 4) {
            return false;
        }
        pos_ += 4;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned codePoint) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    std::optional<Json> parseNumber() {
        double value = 0.0;
        const auto [ptr, ec] = std::from_chars(text_.data() + pos_, text_.data() + text_.size(), value);
        if (ec != std::errc()) {
            return std::nullopt;
        }
        pos_ = static_cast<size_t>(ptr - text_.data());
        return Json(value);
    }

    std::optional<Json> parseLiteral(std::string_view literal, Json value) {
        if (text_.substr(pos_, literal.size()) != literal) {
            return std::nullopt;
        }
        pos_ += literal.size();
        return value;
    }

    void skipWhitespace() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\n' || text_[pos_] == '\r' || text_[pos_] == '\t')) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    static constexpr int kMaxDepth = 64;
    std::string_view text_;
    size_t pos_ = 0;
    int depth_ = 0;
};

}  // namespace

bool Json::asBool(bool fallback) const {
    return isBool() ? std::get<bool>(value_) : fallback;
}

double Json::asNumber(double fallback) const {
    return isNumber() ? std::get<double>(value_) : fallback;
}

int Json::asInt(int fallback) const {
    return isNumber() ? static_cast<int>(std::get<double>(value_)) : fallback;
}

std::string Json::asString(std::string_view fallback) const {
    return isString() ? std::get<std::string>(value_) : std::string(fallback);
}

const Json::Array& Json::asArray() const {
    return isArray() ? std::get<Array>(value_) : kEmptyArray;
}

const Json::Object& Json::asObject() const {
    return isObject() ? std::get<Object>(value_) : kEmptyObject;
}

Json::Array& Json::asArray() {
    if (!isArray()) {
        value_ = Array{};
    }
    return std::get<Array>(value_);
}

Json::Object& Json::asObject() {
    if (!isObject()) {
        value_ = Object{};
    }
    return std::get<Object>(value_);
}

Json& Json::operator[](std::string_view key) {
    auto& object = asObject();
    auto it = object.find(key);
    if (it == object.end()) {
        it = object.emplace(std::string(key), Json()).first;
    }
    return it->second;
}

const Json* Json::find(std::string_view key) const {
    if (!isObject()) {
        return nullptr;
    }
    const auto& object = std::get<Object>(value_);
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

void Json::push(Json value) {
    asArray().push_back(std::move(value));
}

size_t Json::size() const {
    if (isArray()) {
        return std::get<Array>(value_).size();
    }
    if (isObject()) {
        return std::get<Object>(value_).size();
    }
    return 0;
}

std::string Json::dump() const {
    std::string out;
    dumpTo(out);
    return out;
}

void Json::dumpTo(std::string& out) const {
    switch (value_.index()) {
        case 0: out += "null"; break;
        case 1: out += std::get<bool>(value_) ? "true" : "false"; break;
        case 2: appendNumber(out, std::get<double>(value_), 6); break;
        case 3: appendEscaped(out, std::get<std::string>(value_)); break;
        case 4: {
            out.push_back('[');
            bool first = true;
            for (const auto& item : std::get<Array>(value_)) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                item.dumpTo(out);
            }
            out.push_back(']');
            break;
        }
        case 5: {
            out.push_back('{');
            bool first = true;
            for (const auto& [key, item] : std::get<Object>(value_)) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                appendEscaped(out, key);
                out.push_back(':');
                item.dumpTo(out);
            }
            out.push_back('}');
            break;
        }
    }
}

std::optional<Json> Json::parse(std::string_view text) {
    // Tolerate a UTF-8 byte order mark written by external editors.
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") {
        text.remove_prefix(3);
    }
    return Parser(text).parseDocument();
}

void appendNumber(std::string& out, double value, int decimals) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        char buffer[32];
        const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<int64_t>(value));
        out.append(buffer, ptr);
        return;
    }
    char buffer[64];
    const auto [ptr, ec] =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, decimals);
    std::string_view text(buffer, static_cast<size_t>(ptr - buffer));
    if (text.find('.') != std::string_view::npos) {
        while (!text.empty() && text.back() == '0') {
            text.remove_suffix(1);
        }
        if (!text.empty() && text.back() == '.') {
            text.remove_suffix(1);
        }
    }
    if (text == "-0") {
        text = "0";
    }
    out.append(text);
}

}  // namespace perfpulse::core
