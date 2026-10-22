#include "core/config.hpp"
#include "test_framework.hpp"

using namespace perfpulse::core;

TEST_CASE("config: round trips through json") {
    Config config;
    config.monitorDevice = "\\\\.\\DISPLAY2";
    config.sampleIntervalMs = 500;
    config.finishAction = FinishAction::OpenReport;
    config.language = Language::English;
    config.alwaysMonitor = {"cyberpunk2077.exe"};
    config.metrics = MetricSet::fpsOnly();

    const auto restored = Config::fromJson(config.toJson());
    CHECK(restored.monitorDevice == config.monitorDevice);
    CHECK(restored.sampleIntervalMs == 500);
    CHECK(restored.finishAction == FinishAction::OpenReport);
    CHECK(restored.language == Language::English);
    CHECK(restored.metrics == MetricSet::fpsOnly());
    CHECK(restored.isAlwaysMonitored("Cyberpunk2077.EXE"));
}

TEST_CASE("config: clamps out of range values") {
    const auto json = Json::parse(R"({"sample_interval_ms": 5, "detection_delay_sec": 99999})");
    const auto config = Config::fromJson(*json);
    CHECK(config.sampleIntervalMs == 250);
    CHECK(config.detectionDelaySec == 300);
}

TEST_CASE("config: built-in exclusions apply") {
    Config config;
    CHECK(config.isNeverMonitored("chrome.exe"));
    CHECK(config.isNeverMonitored("Discord.exe"));
    CHECK(!config.isNeverMonitored("eldenring.exe"));
}

TEST_CASE("metrics: keys are unique and resolvable") {
    for (const auto& info : allMetrics()) {
        const auto metric = metricFromKey(info.key);
        CHECK(metric.has_value());
        CHECK(*metric == info.metric);
    }
    CHECK(MetricSet::everything().has(Metric::WallClock));
    CHECK(!MetricSet::defaults().has(Metric::GpuHotspot));
}
