#include "sensors/hwinfo_source.hpp"

#include <windows.h>

#include <cstring>
#include <string_view>

#include "core/log.hpp"

namespace perfpulse::sensors {

namespace {

#pragma pack(push, 1)
struct SharedHeader {
    DWORD signature;  // 'HWiS' when alive, 'DEAD' when HWiNFO stopped sharing
    DWORD version;
    DWORD revision;
    __int64 pollTime;
    DWORD sensorSectionOffset;
    DWORD sensorElementSize;
    DWORD sensorElementCount;
    DWORD readingSectionOffset;
    DWORD readingElementSize;
    DWORD readingElementCount;
};

struct SharedReading {
    DWORD type;
    DWORD sensorIndex;
    DWORD readingId;
    char labelOriginal[128];
    char labelUser[128];
    char unit[16];
    double value;
    double valueMin;
    double valueMax;
    double valueAvg;
};
#pragma pack(pop)

constexpr DWORD kSignatureAlive = 0x53695748;  // "HWiS"
constexpr DWORD kReadingTemperature = 1;
constexpr DWORD kReadingPower = 5;

const SharedHeader* header(const unsigned char* view) {
    return reinterpret_cast<const SharedHeader*>(view);
}

const SharedReading* readingAt(const unsigned char* view, long index) {
    const auto* h = header(view);
    return reinterpret_cast<const SharedReading*>(view + h->readingSectionOffset +
                                                  static_cast<size_t>(index) * h->readingElementSize);
}

}  // namespace

std::unique_ptr<HwinfoSource> HwinfoSource::create() {
    std::unique_ptr<HwinfoSource> source(new HwinfoSource());
    source->mapping_.reset(OpenFileMappingW(FILE_MAP_READ, FALSE, L"Global\\HWiNFO_SENS_SM2"));
    if (!source->mapping_) {
        return nullptr;
    }
    source->view_ = static_cast<const unsigned char*>(MapViewOfFile(source->mapping_.get(), FILE_MAP_READ, 0, 0, 0));
    if (source->view_ == nullptr || header(source->view_)->signature != kSignatureAlive ||
        !source->locateReadings()) {
        return nullptr;
    }
    core::log::info("HWiNFO shared memory found");
    return source;
}

HwinfoSource::~HwinfoSource() {
    if (view_ != nullptr) {
        UnmapViewOfFile(view_);
    }
}

bool HwinfoSource::locateReadings() {
    const auto* h = header(view_);
    for (long i = 0; i < static_cast<long>(h->readingElementCount); ++i) {
        const auto* reading = readingAt(view_, i);
        const std::string_view label(reading->labelOriginal);
        if (temperatureIndex_ < 0 && reading->type == kReadingTemperature &&
            (label == "CPU Package" || label == "CPU (Tctl/Tdie)" || label == "CPU Die (average)")) {
            temperatureIndex_ = i;
        }
        if (powerIndex_ < 0 && reading->type == kReadingPower && label == "CPU Package Power") {
            powerIndex_ = i;
        }
    }
    return temperatureIndex_ >= 0 || powerIndex_ >= 0;
}

void HwinfoSource::read(session::SensorSample& sample) {
    if (header(view_)->signature != kSignatureAlive) {
        return;  // HWiNFO closed or shared memory expired.
    }
    if (temperatureIndex_ >= 0) {
        sample.cpuTemperature = static_cast<float>(readingAt(view_, temperatureIndex_)->value);
    }
    if (powerIndex_ >= 0) {
        sample.cpuPower = static_cast<float>(readingAt(view_, powerIndex_)->value);
    }
}

}  // namespace perfpulse::sensors
