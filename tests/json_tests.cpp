#include "core/json.hpp"
#include "test_framework.hpp"

using perfpulse::core::Json;

TEST_CASE("json: parses nested documents") {
    const auto json = Json::parse(R"({"a": [1, 2.5, true, null], "b": {"c": "dé"}})");
    CHECK(json.has_value());
    CHECK(json->find("a")->size() == 4);
    CHECK_NEAR(json->find("a")->asArray()[1].asNumber(), 2.5, 1e-9);
    CHECK(json->find("b")->find("c")->asString() == "d\xC3\xA9");
}

TEST_CASE("json: rejects malformed input") {
    CHECK(!Json::parse("{\"a\": }").has_value());
    CHECK(!Json::parse("[1, 2").has_value());
    CHECK(!Json::parse("{} trailing").has_value());
}

TEST_CASE("json: round trips and escapes html") {
    Json json = Json::object();
    json["text"] = "</script><b>&";
    json["n"] = 1.25;
    json["nan"] = std::nan("");
    const auto text = json.dump();
    CHECK(text.find("</script>") == std::string::npos);
    CHECK(text.find("\"nan\":null") != std::string::npos);
    const auto parsed = Json::parse(text);
    CHECK(parsed.has_value());
    CHECK(parsed->find("text")->asString() == "</script><b>&");
}

TEST_CASE("json: compact number formatting") {
    std::string out;
    perfpulse::core::appendNumber(out, 3.0, 2);
    out += ' ';
    perfpulse::core::appendNumber(out, 3.10, 2);
    out += ' ';
    perfpulse::core::appendNumber(out, -0.001, 2);
    CHECK(out == "3 3.1 0");
}
