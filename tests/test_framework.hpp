#pragma once

// Tiny self-registering test framework. Keeps the build free of external
// dependencies; swap for GoogleTest/Catch2 if the suite grows.

#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace perfpulse::testing {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

}  // namespace perfpulse::testing

#define PP_CONCAT_INNER(a, b) a##b
#define PP_CONCAT(a, b) PP_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                        \
    static void PP_CONCAT(test_body_, __LINE__)();                                             \
    static const ::perfpulse::testing::Registrar PP_CONCAT(test_registrar_, __LINE__)(        \
        name, &PP_CONCAT(test_body_, __LINE__));                                               \
    static void PP_CONCAT(test_body_, __LINE__)()

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            ::perfpulse::testing::reportFailure(__FILE__, __LINE__, "CHECK(" #condition ")");  \
        }                                                                                      \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                                \
    do {                                                                                       \
        const double pp_actual = static_cast<double>(actual);                                  \
        const double pp_expected = static_cast<double>(expected);                              \
        if (!(std::fabs(pp_actual - pp_expected) <= (tolerance))) {                            \
            std::ostringstream pp_message;                                                     \
            pp_message << "CHECK_NEAR(" #actual ", " #expected "): got " << pp_actual          \
                       << ", expected " << pp_expected;                                        \
            ::perfpulse::testing::reportFailure(__FILE__, __LINE__, pp_message.str());         \
        }                                                                                      \
    } while (false)
