#include <cstdio>

#include "test_framework.hpp"

namespace perfpulse::testing {

namespace {
int gFailures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void reportFailure(const char* file, int line, const std::string& message) {
    ++gFailures;
    std::fprintf(stderr, "  FAILED %s(%d): %s\n", file, line, message.c_str());
}

}  // namespace perfpulse::testing

int main() {
    using namespace perfpulse::testing;
    int failedTests = 0;
    for (const auto& test : registry()) {
        const int before = gFailures;
        test.body();
        const bool passed = gFailures == before;
        failedTests += passed ? 0 : 1;
        std::printf("[%s] %s\n", passed ? " OK " : "FAIL", test.name);
    }
    std::printf("\n%zu tests, %d failed\n", registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
