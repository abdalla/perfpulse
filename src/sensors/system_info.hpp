#pragma once

#include "session/session_data.hpp"

namespace perfpulse::sensors {

// Static hardware description (CPU model and core topology, RAM, Windows
// build). GPU fields are filled by the GPU backend.
session::HardwareInfo querySystemInfo();

// True when the process runs with an elevated token.
bool isProcessElevated();

}  // namespace perfpulse::sensors
