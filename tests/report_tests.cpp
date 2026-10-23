#include "analysis/session_analysis.hpp"
#include "report/html_report_writer.hpp"
#include "report/report_json.hpp"
#include "report/report_reanalysis.hpp"
#include "test_framework.hpp"

using namespace perfpulse;

namespace {

session::SessionData sampleSession() {
    session::SessionData data;
    data.info.gameName = "Test: Game/Name?";
    data.info.exeName = "test.exe";
    data.info.startUnixMs = 1761170000000;
    data.metrics = core::MetricSet::everything();
    data.sampleIntervalSec = 1.0;
    for (int i = 0; i <= 6000; ++i) {
        data.frameTimesSec.push_back(i / 100.0);
    }
    for (int i = 1; i <= 60; ++i) {
        session::SensorSample s;
        s.timeSec = i;
        s.gpuUsage = 97.0f;
        s.gpuPower = 250.0f;
        data.samples.push_back(s);
    }
    data.durationSec = 60.0;
    return data;
}

}  // namespace

TEST_CASE("report: payload carries the headline numbers") {
    const auto data = sampleSession();
    const auto analysis = analysis::analyzeSession(data);
    const auto payload = report::buildReportPayload(data, analysis, "pt-BR");
    CHECK_NEAR(payload.find("frames")->find("avg_fps")->asNumber(), 100.0, 0.1);
    CHECK(payload.find("bottleneck")->find("offender")->asString() == "gpu");
    CHECK(payload.find("series")->find("cpu_power")->isNull());
    CHECK(payload.find("series")->find("gpu_usage")->size() == 60);
}

TEST_CASE("report: reanalysis rebuilds the bottleneck from the embedded series") {
    // 100 FPS pinned (a cap) with a saturated thread and an idle-ish GPU.
    auto data = sampleSession();
    for (auto& s : data.samples) {
        s.gpuUsage = 70.0f;
        s.gameBusiestThread = 92.0f;
    }
    const auto analysis = analysis::analyzeSession(data);
    auto payload = report::buildReportPayload(data, analysis, "pt-BR");
    payload["bottleneck"]["offender"] = "cpu";  // what an older version would have said

    const auto html = report::renderTemplate("<html><script id=\"perfpulse-data\" type=\"application/json\">"
                                             "__PERFPULSE_DATA__</script></html>",
                                             payload);
    const auto extracted = report::extractPayload(*html);
    CHECK(extracted.has_value());
    const auto updated = report::reanalyzePayload(*extracted);
    CHECK(updated.has_value());
    CHECK(updated->find("bottleneck")->find("offender")->asString() == "frame_cap");
    CHECK(updated->find("series")->find("bottleneck")->size() == data.samples.size());
    CHECK_NEAR(updated->find("frames")->find("avg_fps")->asNumber(), 100.0, 0.1);  // frame stats untouched
    CHECK(!report::extractPayload("<html>no data</html>").has_value());
}

TEST_CASE("report: template rendering and file naming") {
    core::Json payload = core::Json::object();
    payload["x"] = 1;
    const auto html = report::renderTemplate("<script>const d = __PERFPULSE_DATA__;</script>", payload);
    CHECK(html.has_value());
    CHECK(*html == "<script>const d = {\"x\":1};</script>");
    CHECK(!report::renderTemplate("<p>no marker</p>", payload).has_value());

    const auto name = report::reportFileName(sampleSession().info);
    CHECK(name.rfind("PerfPulse_", 0) == 0);
    CHECK(name.find('/') == std::string::npos);
    CHECK(name.find('?') == std::string::npos);
    CHECK(name.size() > 5 && name.substr(name.size() - 5) == ".html");
}
