#include "sensors/generic_gpu.hpp"

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <map>

#include "core/log.hpp"
#include "core/strings.hpp"

namespace perfpulse::sensors {

namespace {

using Microsoft::WRL::ComPtr;
using NtStatus = LONG;

// Mirrors of the d3dkmthk.h structures we need. Declared locally so the
// build does not depend on DDK-style headers.
struct KmtOpenAdapterFromLuid {
    LUID adapterLuid;
    UINT adapter;
};
struct KmtQueryAdapterInfo {
    UINT adapter;
    int type;
    void* privateData;
    UINT privateDataSize;
};
struct KmtCloseAdapter {
    UINT adapter;
};
struct KmtAdapterPerfData {
    UINT32 physicalAdapterIndex;
    ULONGLONG memoryFrequency;
    ULONGLONG maxMemoryFrequency;
    ULONGLONG maxMemoryFrequencyOc;
    ULONGLONG memoryBandwidth;
    ULONGLONG pcieBandwidth;
    ULONG fanRpm;
    ULONG power;        // tenths of a percent of TDP
    ULONG temperature;  // deci-Celsius
    UCHAR powerStateOverride;
};
struct KmtNodePerfData {
    UINT32 nodeOrdinal;
    UINT32 physicalAdapterIndex;
    ULONGLONG frequency;
    ULONGLONG maxFrequency;
    ULONGLONG maxFrequencyOc;
    ULONG voltage;  // mV
    ULONG voltageMax;
    ULONG voltageMaxOc;
    ULONGLONG maxTransitionLatency;
};
constexpr int kKmtQueryNodePerfData = 61;
constexpr int kKmtQueryAdapterPerfData = 62;

using OpenAdapterFn = NtStatus(APIENTRY*)(KmtOpenAdapterFromLuid*);
using QueryAdapterInfoFn = NtStatus(APIENTRY*)(const KmtQueryAdapterInfo*);
using CloseAdapterFn = NtStatus(APIENTRY*)(const KmtCloseAdapter*);

std::string formatDriverVersion(LARGE_INTEGER version) {
    char text[48];
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", HIWORD(version.HighPart), LOWORD(version.HighPart),
                  HIWORD(version.LowPart), LOWORD(version.LowPart));
    return text;
}

}  // namespace

std::unique_ptr<GenericGpu> GenericGpu::create() {
    std::unique_ptr<GenericGpu> gpu(new GenericGpu());
    return gpu->initialize() ? std::move(gpu) : nullptr;
}

GenericGpu::~GenericGpu() {
    if (adapterHandle_ != 0 && closeAdapter_ != nullptr) {
        KmtCloseAdapter close{adapterHandle_};
        reinterpret_cast<CloseAdapterFn>(closeAdapter_)(&close);
    }
}

bool GenericGpu::initialize() {
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
        return false;
    }
    // The high-performance adapter is the discrete GPU on hybrid laptops.
    ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                    IID_PPV_ARGS(&adapter)))) {
        return false;
    }
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        return false;
    }
    name_ = core::toUtf8(desc.Description);
    luid_ = desc.AdapterLuid;
    vramTotalGb_ = static_cast<float>(static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0 * 1024.0));
    LARGE_INTEGER umdVersion{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion))) {
        driver_ = formatDriverVersion(umdVersion);
    }
    wchar_t tag[64];
    swprintf_s(tag, L"luid_0x%08X_0x%08X", static_cast<unsigned>(luid_.HighPart), luid_.LowPart);
    luidTag_ = tag;

    gdi_.reset(LoadLibraryExW(L"gdi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    const auto open = gdi_.proc<OpenAdapterFn>("D3DKMTOpenAdapterFromLuid");
    queryAdapterInfo_ = reinterpret_cast<void*>(gdi_.proc<QueryAdapterInfoFn>("D3DKMTQueryAdapterInfo"));
    closeAdapter_ = reinterpret_cast<void*>(gdi_.proc<CloseAdapterFn>("D3DKMTCloseAdapter"));
    if (open != nullptr) {
        KmtOpenAdapterFromLuid request{luid_, 0};
        if (open(&request) >= 0) {
            adapterHandle_ = request.adapter;
        }
    }

    if (pdh_.open()) {
        engineCounter_ = pdh_.add(L"\\GPU Engine(*)\\Utilization Percentage");
        memoryCounter_ = pdh_.add(L"\\GPU Adapter Memory(*)\\Dedicated Usage");
        pdh_.collect();  // Rate counters need a first sample.
    }
    core::log::info("Generic GPU backend: " + name_);
    return true;
}

void GenericGpu::read(session::SensorSample& s, const SensorRequest& request) {
    using core::Metric;
    s.vramTotalGb = vramTotalGb_;

    if (pdh_.collect()) {
        // Sum per engine across processes, then take the busiest engine, like Task Manager.
        std::map<std::wstring, double> engines;
        for (const auto& [instance, value] : pdh_.values(engineCounter_)) {
            if (instance.find(luidTag_) == std::wstring::npos) {
                continue;
            }
            const auto engPos = instance.find(L"_eng_");
            if (engPos != std::wstring::npos) {
                engines[instance.substr(engPos)] += value;
            }
        }
        double busiest = 0.0;
        for (const auto& [engine, value] : engines) {
            busiest = std::max(busiest, value);
        }
        if (!engines.empty()) {
            s.gpuUsage = static_cast<float>(std::min(busiest, 100.0));
        }
        for (const auto& [instance, bytes] : pdh_.values(memoryCounter_)) {
            if (instance.find(luidTag_) != std::wstring::npos) {
                s.vramUsedGb = static_cast<float>(bytes / (1024.0 * 1024.0 * 1024.0));
                break;
            }
        }
    }

    if (adapterHandle_ == 0 || queryAdapterInfo_ == nullptr) {
        return;
    }
    const auto query = reinterpret_cast<QueryAdapterInfoFn>(queryAdapterInfo_);
    KmtAdapterPerfData adapterData{};
    KmtQueryAdapterInfo adapterQuery{adapterHandle_, kKmtQueryAdapterPerfData, &adapterData, sizeof(adapterData)};
    if (query(&adapterQuery) >= 0) {
        if (adapterData.temperature > 0) {
            s.gpuTemperature = static_cast<float>(adapterData.temperature) / 10.0f;
        }
        if (request.wants(Metric::GpuMemoryClock) && adapterData.memoryFrequency > 0) {
            s.gpuMemoryClock = static_cast<float>(static_cast<double>(adapterData.memoryFrequency) / 1e6);
        }
        if (request.wants(Metric::GpuFan) && adapterData.fanRpm > 0) {
            s.gpuFanRpm = static_cast<float>(adapterData.fanRpm);
        }
    }
    if (request.wants(Metric::GpuCoreClock) || request.wants(Metric::GpuVoltage)) {
        KmtNodePerfData nodeData{};
        KmtQueryAdapterInfo nodeQuery{adapterHandle_, kKmtQueryNodePerfData, &nodeData, sizeof(nodeData)};
        if (query(&nodeQuery) >= 0) {
            if (nodeData.frequency > 0) {
                s.gpuCoreClock = static_cast<float>(static_cast<double>(nodeData.frequency) / 1e6);
            }
            if (nodeData.voltage > 0) {
                s.gpuVoltage = static_cast<float>(nodeData.voltage) / 1000.0f;
            }
        }
    }
}

}  // namespace perfpulse::sensors
