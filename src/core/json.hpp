#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace perfpulse::core {

// Minimal JSON document model. Covers what PerfPulse needs (config, history,
// report payloads) without pulling a third-party dependency into the build.
class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool value) : value_(value) {}
    Json(int value) : value_(static_cast<double>(value)) {}
    Json(int64_t value) : value_(static_cast<double>(value)) {}
    Json(uint32_t value) : value_(static_cast<double>(value)) {}
    Json(long value) : value_(static_cast<double>(value)) {}
    Json(unsigned long value) : value_(static_cast<double>(value)) {}
    Json(uint64_t value) : value_(static_cast<double>(value)) {}
    Json(double value) : value_(value) {}
    Json(float value) : value_(static_cast<double>(value)) {}
    Json(const char* value) : value_(std::string(value)) {}
    Json(std::string value) : value_(std::move(value)) {}
    Json(std::string_view value) : value_(std::string(value)) {}
    Json(Array value) : value_(std::move(value)) {}
    Json(Object value) : value_(std::move(value)) {}

    static Json array() { return Json(Array{}); }
    static Json object() { return Json(Object{}); }

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isNumber() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<Array>(value_); }
    bool isObject() const { return std::holds_alternative<Object>(value_); }

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const;
    std::string asString(std::string_view fallback = {}) const;
    const Array& asArray() const;
    const Object& asObject() const;
    Array& asArray();
    Object& asObject();

    // Object access. operator[] converts a null value into an object.
    Json& operator[](std::string_view key);
    const Json* find(std::string_view key) const;
    bool contains(std::string_view key) const { return find(key) != nullptr; }

    void push(Json value);
    size_t size() const;

    // Serializes the document. Non-finite numbers become null. The output is
    // safe to embed inside an HTML <script> element.
    std::string dump() const;
    void dumpTo(std::string& out) const;

    static std::optional<Json> parse(std::string_view text);

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
};

// Appends a number with a bounded number of decimals, trimming trailing zeros.
// Used for large numeric series to keep reports compact.
void appendNumber(std::string& out, double value, int decimals);

}  // namespace perfpulse::core
