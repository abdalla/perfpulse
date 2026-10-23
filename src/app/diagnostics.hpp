#pragma once

#include <filesystem>

namespace perfpulse::app {

// "PerfPulse.exe --diag [--out file.txt]": prints which sensors and capture
// paths work on this machine, the live per-process present rate and a few
// sensor samples. Writes to the console, or to a file when --out is given.
int runDiagnostics(const std::filesystem::path& outputFile);

// "PerfPulse.exe --demo-report [--out file.txt]": writes a report from
// synthetic data and prints its path.
int runDemoReport(const std::filesystem::path& outputFile);

// "PerfPulse.exe --reanalyze report.html [--to new.html] [--out log.txt]":
// re-runs the bottleneck analysis of an existing report with the current
// rules and template, writing it in place unless --to is given, and updates
// the history entry and index.html next to it.
int runReanalyze(const std::filesystem::path& report, const std::filesystem::path& destination,
                 const std::filesystem::path& outputFile);

}  // namespace perfpulse::app
