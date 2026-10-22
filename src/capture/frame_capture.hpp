#pragma once

#include <windows.h>
#include <evntrace.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace perfpulse::capture {

// Where a present timestamp came from, in order of preference.
enum class PresentSource : uint8_t { Dxgi, D3d9, DxgKrnl, Count };

inline constexpr size_t kPresentSourceCount = static_cast<size_t>(PresentSource::Count);

std::string_view presentSourceName(PresentSource source);

struct ProcessActivity {
    DWORD processId = 0;
    uint32_t presents = 0;  // presents since the previous takeActivity() call
};

struct RecordedFrames {
    std::vector<double> timesSec;  // relative to the recording origin
    PresentSource source = PresentSource::Dxgi;
    int dominantSyncInterval = -1;
};

// Real-time ETW consumer that timestamps every Present call of every process,
// the same technique PresentMon uses, without injecting into the game.
//
// Two jobs share one trace session:
//  * activity counters per process, used by the game detector;
//  * full per-frame timestamps for the process being recorded.
//
// Requires administrator rights (or membership in "Performance Log Users").
class FrameCapture {
public:
    enum class Status { Stopped, Running, AccessDenied, Failed };

    FrameCapture() = default;
    ~FrameCapture();
    FrameCapture(const FrameCapture&) = delete;
    FrameCapture& operator=(const FrameCapture&) = delete;

    bool start();
    void stop();
    Status status() const { return status_.load(); }

    std::vector<ProcessActivity> takeActivity();

    void beginRecording(DWORD processId, int64_t originQpc);
    RecordedFrames endRecording();
    bool isRecording() const { return recordingPid_.load() != 0; }

    uint64_t eventsLost() const;

    // Diagnostics: events seen per (provider, event id) since start.
    std::map<std::pair<int, USHORT>, uint64_t> eventCounts() const;

private:
    static void WINAPI onEventRecord(PEVENT_RECORD record);
    void handleEvent(const EVENT_RECORD& record);
    bool enableProviders();

    std::atomic<Status> status_{Status::Stopped};
    TRACEHANDLE sessionHandle_ = 0;
    TRACEHANDLE consumerHandle_ = INVALID_PROCESSTRACE_HANDLE;
    std::thread consumerThread_;

    mutable std::mutex mutex_;
    std::unordered_map<DWORD, std::array<uint32_t, kPresentSourceCount>> activity_;
    std::map<std::pair<int, USHORT>, uint64_t> eventCounts_;

    std::atomic<DWORD> recordingPid_{0};
    int64_t originQpc_ = 0;
    std::array<std::vector<int64_t>, kPresentSourceCount> recorded_;
    std::array<uint32_t, 5> syncIntervals_{};  // DXGI SyncInterval 0..4
};

}  // namespace perfpulse::capture
