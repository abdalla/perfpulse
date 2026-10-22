#pragma once

namespace perfpulse::app {

// "Start with Windows" through a Task Scheduler logon task that runs with
// highest privileges, so the elevated app starts without a UAC prompt.
bool setAutostart(bool enabled);
bool isAutostartEnabled();

}  // namespace perfpulse::app
