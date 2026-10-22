#pragma once

#include "session/session_data.hpp"

namespace perfpulse::report {

// A synthetic 18-minute session that exercises every part of the report:
// loading screen, GPU-bound and CPU-bound phases, alt-tab, a frame cap and
// shader-compilation stutters. Deterministic (fixed seed).
session::SessionData makeDemoSession(const core::MetricSet& metrics);

}  // namespace perfpulse::report
