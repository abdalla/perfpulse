#include "sensors/cpu_sensors.hpp"

#include <winternl.h>

#include <algorithm>
#include <map>

#include "core/win_handle.hpp"

namespace perfpulse::sensors {

namespace {

constexpr int kSystemProcessInformation = 5;
constexpr int kSystemProcessorPerformanceInformation = 8;
constexpr LONG kStatusInfoLengthMismatch = static_cast<LONG>(0xC0000004);

using NtQuerySystemInformationFn = LONG(NTAPI*)(int, PVOID, ULONG, PULONG);

struct ProcessorPerformance {
    LARGE_INTEGER idleTime;
    LARGE_INTEGER kernelTime;  // includes idle time
    LARGE_INTEGER userTime;
    LARGE_INTEGER dpcTime;
    LARGE_INTEGER interruptTime;
    ULONG interruptCount;
};

// Full layouts of the structures winternl.h only declares partially.
struct ThreadInformation {
    LARGE_INTEGER kernelTime;
    LARGE_INTEGER userTime;
    LARGE_INTEGER createTime;
    ULONG waitTime;
    PVOID startAddress;
    HANDLE uniqueProcess;
    HANDLE uniqueThread;
    LONG priority;
    LONG basePriority;
    ULONG contextSwitches;
    ULONG threadState;
    ULONG waitReason;
};

struct ProcessInformation {
    ULONG nextEntryOffset;
    ULONG numberOfThreads;
    LARGE_INTEGER workingSetPrivateSize;
    ULONG hardFaultCount;
    ULONG numberOfThreadsHighWatermark;
    ULONGLONG cycleTime;
    LARGE_INTEGER createTime;
    LARGE_INTEGER userTime;
    LARGE_INTEGER kernelTime;
    UNICODE_STRING imageName;
    LONG basePriority;
    HANDLE uniqueProcessId;
    HANDLE inheritedFromUniqueProcessId;
    ULONG handleCount;
    ULONG sessionId;
    ULONG_PTR uniqueProcessKey;
    SIZE_T peakVirtualSize;
    SIZE_T virtualSize;
    ULONG pageFaultCount;
    SIZE_T peakWorkingSetSize;
    SIZE_T workingSetSize;
    SIZE_T quotaPeakPagedPoolUsage;
    SIZE_T quotaPagedPoolUsage;
    SIZE_T quotaPeakNonPagedPoolUsage;
    SIZE_T quotaNonPagedPoolUsage;
    SIZE_T pagefileUsage;
    SIZE_T peakPagefileUsage;
    SIZE_T privatePageCount;
    LARGE_INTEGER readOperationCount;
    LARGE_INTEGER writeOperationCount;
    LARGE_INTEGER otherOperationCount;
    LARGE_INTEGER readTransferCount;
    LARGE_INTEGER writeTransferCount;
    LARGE_INTEGER otherTransferCount;
};
static_assert(sizeof(ProcessInformation) == 0x100, "SYSTEM_PROCESS_INFORMATION layout (x64)");
static_assert(sizeof(ThreadInformation) == 0x50, "SYSTEM_THREAD_INFORMATION layout (x64)");

NtQuerySystemInformationFn ntQuerySystemInformation() {
    static const auto fn = reinterpret_cast<NtQuerySystemInformationFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation"));
    return fn;
}

int64_t fileTimeNow() {
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    return (static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
}

}  // namespace

CpuSensors::CpuSensors() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    logicalProcessors_ = static_cast<int>(std::max<DWORD>(1, info.dwNumberOfProcessors));

    if (clockQuery_.open()) {
        performanceCounter_ = clockQuery_.add(L"\\Processor Information(*)\\% Processor Performance");
        frequencyCounter_ = clockQuery_.add(L"\\Processor Information(*)\\Processor Frequency");
        clockQuery_.collect();
    }
    session::SensorSample primer;
    readProcessorLoad(primer);
}

void CpuSensors::read(session::SensorSample& sample, const SensorRequest& request) {
    using core::Metric;
    readProcessorLoad(sample);
    if (request.gameProcessId != 0) {
        readGameThreads(sample, request.gameProcessId);
    }
    if (request.wants(Metric::CpuClockAverage) || request.wants(Metric::CpuClockMax)) {
        readClocks(sample);
    }
}

void CpuSensors::readProcessorLoad(session::SensorSample& sample) {
    const auto query = ntQuerySystemInformation();
    if (query == nullptr) {
        return;
    }
    std::vector<ProcessorPerformance> cores(static_cast<size_t>(logicalProcessors_));
    ULONG returned = 0;
    if (query(kSystemProcessorPerformanceInformation, cores.data(),
              static_cast<ULONG>(cores.size() * sizeof(ProcessorPerformance)), &returned) < 0) {
        return;
    }
    cores.resize(returned / sizeof(ProcessorPerformance));

    const bool hasPrevious = previousCores_.size() == cores.size();
    double totalBusy = 0.0;
    double totalTime = 0.0;
    double busiest = 0.0;
    std::vector<CoreTimes> current(cores.size());
    for (size_t i = 0; i < cores.size(); ++i) {
        current[i].idle = cores[i].idleTime.QuadPart;
        current[i].total = cores[i].kernelTime.QuadPart + cores[i].userTime.QuadPart;
        if (!hasPrevious) {
            continue;
        }
        const double idle = static_cast<double>(current[i].idle - previousCores_[i].idle);
        const double total = static_cast<double>(current[i].total - previousCores_[i].total);
        if (total <= 0.0) {
            continue;
        }
        const double busy = std::clamp(total - idle, 0.0, total);
        totalBusy += busy;
        totalTime += total;
        busiest = std::max(busiest, busy / total * 100.0);
    }
    previousCores_ = std::move(current);
    if (hasPrevious && totalTime > 0.0) {
        sample.cpuUsage = static_cast<float>(totalBusy / totalTime * 100.0);
        sample.cpuBusiestCore = static_cast<float>(busiest);
    }
}

void CpuSensors::readGameThreads(session::SensorSample& sample, DWORD gameProcessId) {
    const auto query = ntQuerySystemInformation();
    if (query == nullptr) {
        return;
    }
    if (trackedProcessId_ != gameProcessId) {
        trackedProcessId_ = gameProcessId;
        previousThreadTimes_.clear();
        previousProcessTime_ = 0;
        previousWallTime_ = 0;
    }
    if (processBuffer_.empty()) {
        processBuffer_.resize(1 << 20);
    }
    ULONG needed = 0;
    LONG status = 0;
    while ((status = query(kSystemProcessInformation, processBuffer_.data(),
                           static_cast<ULONG>(processBuffer_.size()), &needed)) == kStatusInfoLengthMismatch) {
        processBuffer_.resize(std::max<size_t>(needed + 64 * 1024, processBuffer_.size() * 2));
    }
    if (status < 0) {
        return;
    }
    const int64_t wallNow = fileTimeNow();

    const BYTE* cursor = processBuffer_.data();
    while (true) {
        const auto* process = reinterpret_cast<const ProcessInformation*>(cursor);
        if (reinterpret_cast<ULONG_PTR>(process->uniqueProcessId) == gameProcessId) {
            const auto* threads = reinterpret_cast<const ThreadInformation*>(cursor + sizeof(ProcessInformation));
            const double wallDelta = static_cast<double>(wallNow - previousWallTime_);
            const bool hasPrevious = previousWallTime_ != 0 && wallDelta > 0.0;
            double busiestThread = 0.0;
            std::unordered_map<uint64_t, int64_t> current;
            current.reserve(process->numberOfThreads);
            for (ULONG i = 0; i < process->numberOfThreads; ++i) {
                const auto id = static_cast<uint64_t>(reinterpret_cast<ULONG_PTR>(threads[i].uniqueThread));
                const int64_t time = threads[i].kernelTime.QuadPart + threads[i].userTime.QuadPart;
                current.emplace(id, time);
                const auto previous = previousThreadTimes_.find(id);
                if (hasPrevious && previous != previousThreadTimes_.end()) {
                    busiestThread =
                        std::max(busiestThread, static_cast<double>(time - previous->second) / wallDelta * 100.0);
                }
            }
            const int64_t processTime = process->kernelTime.QuadPart + process->userTime.QuadPart;
            if (hasPrevious) {
                sample.gameBusiestThread = static_cast<float>(std::min(busiestThread, 100.0));
                sample.gameCpuUsage = static_cast<float>(
                    std::clamp(static_cast<double>(processTime - previousProcessTime_) / wallDelta /
                                   logicalProcessors_ * 100.0,
                               0.0, 100.0));
            }
            sample.gameWorkingSetGb =
                static_cast<float>(static_cast<double>(process->workingSetSize) / (1024.0 * 1024.0 * 1024.0));
            previousThreadTimes_ = std::move(current);
            previousProcessTime_ = processTime;
            previousWallTime_ = wallNow;
            return;
        }
        if (process->nextEntryOffset == 0) {
            return;
        }
        cursor += process->nextEntryOffset;
    }
}

void CpuSensors::readClocks(session::SensorSample& sample) {
    if (!clockQuery_.collect()) {
        return;
    }
    // Instances are "group,core" plus "_Total" / "group,_Total" aggregates.
    std::map<std::wstring, double> nominal;
    for (const auto& [instance, mhz] : clockQuery_.values(frequencyCounter_)) {
        nominal[instance] = mhz;
    }
    double sum = 0.0;
    double max = 0.0;
    int count = 0;
    for (const auto& [instance, percent] : clockQuery_.values(performanceCounter_)) {
        if (instance.find(L"_Total") != std::wstring::npos) {
            continue;
        }
        const auto base = nominal.find(instance);
        if (base == nominal.end() || base->second <= 0.0) {
            continue;
        }
        const double mhz = base->second * percent / 100.0;
        sum += mhz;
        max = std::max(max, mhz);
        ++count;
    }
    if (count > 0) {
        sample.cpuClockAverage = static_cast<float>(sum / count);
        sample.cpuClockMax = static_cast<float>(max);
    }
}

}  // namespace perfpulse::sensors
