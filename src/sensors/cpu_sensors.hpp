#pragma once

#include <windows.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "sensors/pdh_query.hpp"
#include "sensors/sensor_request.hpp"
#include "session/session_data.hpp"

namespace perfpulse::sensors {

// CPU load and clocks without any driver:
//  * per-logical-processor load from NtQuerySystemInformation;
//  * the game's per-thread CPU time, to spot a saturated main/render thread;
//  * effective clocks from the "% Processor Performance" counters (turbo-aware).
class CpuSensors {
public:
    CpuSensors();

    void read(session::SensorSample& sample, const SensorRequest& request);

private:
    void readProcessorLoad(session::SensorSample& sample);
    void readGameThreads(session::SensorSample& sample, DWORD gameProcessId);
    void readClocks(session::SensorSample& sample);

    struct CoreTimes {
        int64_t idle = 0;
        int64_t total = 0;
    };

    std::vector<CoreTimes> previousCores_;
    std::unordered_map<uint64_t, int64_t> previousThreadTimes_;  // thread id -> kernel+user (100 ns)
    int64_t previousProcessTime_ = 0;
    int64_t previousWallTime_ = 0;
    DWORD trackedProcessId_ = 0;
    std::vector<BYTE> processBuffer_;
    int logicalProcessors_ = 1;

    PdhQuery clockQuery_;
    int performanceCounter_ = -1;
    int frequencyCounter_ = -1;
};

}  // namespace perfpulse::sensors
