#include "capture/frame_capture.hpp"

#include <evntcons.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

#include "core/log.hpp"

namespace perfpulse::capture {

namespace {

constexpr wchar_t kSessionName[] = L"PerfPulse-FrameCapture";

// Microsoft-Windows-DXGI
constexpr GUID kDxgiProvider = {0xCA11C036, 0x0102, 0x4A2D, {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}};
// Microsoft-Windows-D3D9
constexpr GUID kD3d9Provider = {0x783ACA0A, 0x790E, 0x4D7F, {0x84, 0x51, 0xAA, 0x85, 0x05, 0x11, 0xC6, 0xB9}};
// Microsoft-Windows-DxgKrnl
constexpr GUID kDxgKrnlProvider = {0x802EC45A, 0x1E99, 0x4B83, {0x99, 0x20, 0x87, 0xC9, 0x82, 0x77, 0xBA, 0x9D}};

constexpr USHORT kDxgiPresentStart = 42;
constexpr USHORT kDxgiPresentMultiplaneOverlayStart = 55;
constexpr USHORT kD3d9PresentStart = 1;
// DxgKrnl present events cover Vulkan/OpenGL and anything that bypasses DXGI.
constexpr USHORT kDxgKrnlPresentHistoryStart = 171;
constexpr USHORT kDxgKrnlPresentHistoryDetailedStart = 215;

// DxgKrnl keywords: Base | Present | Performance channel.
constexpr ULONGLONG kDxgKrnlKeywords = 0x1ull | 0x8000000ull | 0x4000000000000000ull;

struct TraceProperties {
    EVENT_TRACE_PROPERTIES properties;
    wchar_t name[64];
};

TraceProperties makeProperties() {
    TraceProperties trace{};
    trace.properties.Wnode.BufferSize = sizeof(TraceProperties);
    trace.properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    trace.properties.Wnode.ClientContext = 1;  // QueryPerformanceCounter timestamps
    trace.properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    trace.properties.LoggerNameOffset = offsetof(TraceProperties, name);
    trace.properties.BufferSize = 64;  // KB
    trace.properties.MinimumBuffers = 8;
    trace.properties.MaximumBuffers = 64;
    trace.properties.FlushTimer = 1;  // seconds
    return trace;
}

bool enableProvider(TRACEHANDLE session, const GUID& provider, ULONGLONG keywords,
                    std::initializer_list<USHORT> eventIds) {
    // Filtering by event id inside ETW drops everything else before it reaches
    // our buffers: this is what keeps the capture cheap.
    std::vector<BYTE> filterBuffer(offsetof(EVENT_FILTER_EVENT_ID, Events) + eventIds.size() * sizeof(USHORT));
    auto* filter = reinterpret_cast<EVENT_FILTER_EVENT_ID*>(filterBuffer.data());
    filter->FilterIn = TRUE;
    filter->Count = static_cast<USHORT>(eventIds.size());
    std::copy(eventIds.begin(), eventIds.end(), filter->Events);

    EVENT_FILTER_DESCRIPTOR descriptor{};
    descriptor.Ptr = reinterpret_cast<ULONGLONG>(filterBuffer.data());
    descriptor.Size = static_cast<ULONG>(filterBuffer.size());
    descriptor.Type = EVENT_FILTER_TYPE_EVENT_ID;

    ENABLE_TRACE_PARAMETERS parameters{};
    parameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    parameters.EnableFilterDesc = &descriptor;
    parameters.FilterDescCount = 1;

    const ULONG status = EnableTraceEx2(session, &provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                        TRACE_LEVEL_INFORMATION, keywords, 0, 0, &parameters);
    return status == ERROR_SUCCESS;
}

int64_t elapsedQpcFrequency() {
    static const int64_t frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }();
    return frequency;
}

}  // namespace

std::string_view presentSourceName(PresentSource source) {
    switch (source) {
        case PresentSource::Dxgi: return "DXGI";
        case PresentSource::D3d9: return "D3D9";
        case PresentSource::DxgKrnl: return "DxgKrnl";
        case PresentSource::Count: break;
    }
    return "";
}

FrameCapture::~FrameCapture() {
    stop();
}

bool FrameCapture::start() {
    if (status_ == Status::Running) {
        return true;
    }

    auto trace = makeProperties();
    ULONG result = StartTraceW(&sessionHandle_, kSessionName, &trace.properties);
    if (result == ERROR_ALREADY_EXISTS) {
        // A previous instance crashed and left the session alive: reclaim it.
        auto stopProperties = makeProperties();
        ControlTraceW(0, kSessionName, &stopProperties.properties, EVENT_TRACE_CONTROL_STOP);
        trace = makeProperties();
        result = StartTraceW(&sessionHandle_, kSessionName, &trace.properties);
    }
    if (result != ERROR_SUCCESS) {
        status_ = result == ERROR_ACCESS_DENIED ? Status::AccessDenied : Status::Failed;
        core::log::error("StartTrace failed: " + std::to_string(result));
        sessionHandle_ = 0;
        return false;
    }

    if (!enableProviders()) {
        core::log::warn("Some ETW providers could not be enabled");
    }

    EVENT_TRACE_LOGFILEW logFile{};
    logFile.LoggerName = const_cast<LPWSTR>(kSessionName);
    logFile.ProcessTraceMode =
        PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logFile.EventRecordCallback = &FrameCapture::onEventRecord;
    logFile.Context = this;
    consumerHandle_ = OpenTraceW(&logFile);
    if (consumerHandle_ == INVALID_PROCESSTRACE_HANDLE) {
        core::log::error("OpenTrace failed: " + std::to_string(GetLastError()));
        stop();
        status_ = Status::Failed;
        return false;
    }

    consumerThread_ = std::thread([this] {
        SetThreadDescription(GetCurrentThread(), L"PerfPulse ETW consumer");
        // Above normal so a busy game cannot starve buffer delivery.
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        TRACEHANDLE handle = consumerHandle_;
        ProcessTrace(&handle, 1, nullptr, nullptr);
    });
    status_ = Status::Running;
    core::log::info("ETW frame capture started");
    return true;
}

bool FrameCapture::enableProviders() {
    bool ok = true;
    ok &= enableProvider(sessionHandle_, kDxgiProvider, 0, {kDxgiPresentStart, kDxgiPresentMultiplaneOverlayStart});
    ok &= enableProvider(sessionHandle_, kD3d9Provider, 0, {kD3d9PresentStart});
    ok &= enableProvider(sessionHandle_, kDxgKrnlProvider, kDxgKrnlKeywords,
                         {kDxgKrnlPresentHistoryStart, kDxgKrnlPresentHistoryDetailedStart});
    return ok;
}

void FrameCapture::stop() {
    if (sessionHandle_ != 0) {
        auto trace = makeProperties();
        ControlTraceW(sessionHandle_, nullptr, &trace.properties, EVENT_TRACE_CONTROL_STOP);
        sessionHandle_ = 0;
    }
    if (consumerHandle_ != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(consumerHandle_);
        consumerHandle_ = INVALID_PROCESSTRACE_HANDLE;
    }
    if (consumerThread_.joinable()) {
        consumerThread_.join();
    }
    if (status_ == Status::Running) {
        status_ = Status::Stopped;
        core::log::info("ETW frame capture stopped");
    }
}

std::vector<ProcessActivity> FrameCapture::takeActivity() {
    std::vector<ProcessActivity> result;
    std::lock_guard lock(mutex_);
    result.reserve(activity_.size());
    for (const auto& [pid, counts] : activity_) {
        // Sources can overlap (a DXGI present also reaches DxgKrnl): take the max, not the sum.
        result.push_back({pid, *std::max_element(counts.begin(), counts.end())});
    }
    activity_.clear();
    return result;
}

void FrameCapture::beginRecording(DWORD processId, int64_t originQpc) {
    std::lock_guard lock(mutex_);
    originQpc_ = originQpc;
    for (auto& frames : recorded_) {
        frames.clear();
        frames.reserve(1 << 16);
    }
    syncIntervals_.fill(0);
    recordingPid_ = processId;
}

RecordedFrames FrameCapture::endRecording() {
    std::lock_guard lock(mutex_);
    recordingPid_ = 0;

    RecordedFrames result;
    size_t maxCount = 0;
    for (const auto& frames : recorded_) {
        maxCount = std::max(maxCount, frames.size());
    }
    // Prefer the API-level source when it saw most of the frames.
    for (size_t i = 0; i < kPresentSourceCount; ++i) {
        if (maxCount > 0 && recorded_[i].size() * 2 >= maxCount) {
            result.source = static_cast<PresentSource>(i);
            break;
        }
    }
    const auto& chosen = recorded_[static_cast<size_t>(result.source)];
    const double frequency = static_cast<double>(elapsedQpcFrequency());
    result.timesSec.reserve(chosen.size());
    for (const int64_t qpc : chosen) {
        result.timesSec.push_back(static_cast<double>(qpc - originQpc_) / frequency);
    }
    // ETW buffers are delivered per CPU, so events can arrive slightly out of order.
    std::sort(result.timesSec.begin(), result.timesSec.end());

    const auto dominant = std::max_element(syncIntervals_.begin(), syncIntervals_.end());
    if (*dominant > 0) {
        result.dominantSyncInterval = static_cast<int>(dominant - syncIntervals_.begin());
    }
    for (auto& frames : recorded_) {
        frames = {};
    }
    return result;
}

uint64_t FrameCapture::eventsLost() const {
    if (sessionHandle_ == 0) {
        return 0;
    }
    auto trace = makeProperties();
    if (ControlTraceW(sessionHandle_, nullptr, &trace.properties, EVENT_TRACE_CONTROL_QUERY) != ERROR_SUCCESS) {
        return 0;
    }
    return trace.properties.EventsLost;
}

std::map<std::pair<int, USHORT>, uint64_t> FrameCapture::eventCounts() const {
    std::lock_guard lock(mutex_);
    return eventCounts_;
}

void WINAPI FrameCapture::onEventRecord(PEVENT_RECORD record) {
    static_cast<FrameCapture*>(record->UserContext)->handleEvent(*record);
}

void FrameCapture::handleEvent(const EVENT_RECORD& record) {
    const auto& header = record.EventHeader;
    PresentSource source;
    if (IsEqualGUID(header.ProviderId, kDxgiProvider)) {
        source = PresentSource::Dxgi;
    } else if (IsEqualGUID(header.ProviderId, kD3d9Provider)) {
        source = PresentSource::D3d9;
    } else if (IsEqualGUID(header.ProviderId, kDxgKrnlProvider)) {
        source = PresentSource::DxgKrnl;
    } else {
        return;
    }
    const DWORD pid = header.ProcessId;
    const auto sourceIndex = static_cast<size_t>(source);

    std::lock_guard lock(mutex_);
    eventCounts_[{static_cast<int>(sourceIndex), header.EventDescriptor.Id}]++;
    activity_[pid][sourceIndex]++;
    // Real-time buffers are flushed about once per second, so presents made
    // just before recording started can still arrive afterwards.
    if (pid != recordingPid_.load(std::memory_order_relaxed) || header.TimeStamp.QuadPart < originQpc_) {
        return;
    }
    recorded_[sourceIndex].push_back(header.TimeStamp.QuadPart);

    // DXGI Present_Start payload: pIDXGISwapChain (pointer), Flags (UInt32), SyncInterval (Int32).
    if (source == PresentSource::Dxgi) {
        const size_t pointerSize = (header.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8;
        if (record.UserDataLength >= pointerSize + 8) {
            int32_t syncInterval = 0;
            std::memcpy(&syncInterval, static_cast<const BYTE*>(record.UserData) + pointerSize + 4,
                        sizeof(syncInterval));
            if (syncInterval >= 0 && syncInterval < static_cast<int32_t>(syncIntervals_.size())) {
                syncIntervals_[static_cast<size_t>(syncInterval)]++;
            }
        }
    }
}

}  // namespace perfpulse::capture
