#include "sensors/memory_sensors.hpp"

#include <windows.h>

namespace perfpulse::sensors {

void readSystemMemory(session::SensorSample& sample) {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        return;
    }
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    sample.ramTotalGb = static_cast<float>(static_cast<double>(status.ullTotalPhys) / kGiB);
    sample.ramUsedGb = static_cast<float>(static_cast<double>(status.ullTotalPhys - status.ullAvailPhys) / kGiB);
}

}  // namespace perfpulse::sensors
