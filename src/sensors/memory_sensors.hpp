#pragma once

#include "session/session_data.hpp"

namespace perfpulse::sensors {

// System RAM in use and installed.
void readSystemMemory(session::SensorSample& sample);

}  // namespace perfpulse::sensors
