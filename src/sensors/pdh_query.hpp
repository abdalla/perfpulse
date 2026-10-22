#pragma once

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <string>
#include <utility>
#include <vector>

namespace perfpulse::sensors {

// Small RAII wrapper around a PDH query with wildcard counters.
class PdhQuery {
public:
    PdhQuery() = default;
    ~PdhQuery() {
        if (query_ != nullptr) {
            PdhCloseQuery(query_);
        }
    }
    PdhQuery(const PdhQuery&) = delete;
    PdhQuery& operator=(const PdhQuery&) = delete;

    bool open() { return PdhOpenQueryW(nullptr, 0, &query_) == ERROR_SUCCESS; }

    // Returns the counter index, or -1. Uses English counter names so it works
    // on localized Windows installs.
    int add(const wchar_t* path) {
        PDH_HCOUNTER counter = nullptr;
        if (query_ == nullptr || PdhAddEnglishCounterW(query_, path, 0, &counter) != ERROR_SUCCESS) {
            return -1;
        }
        counters_.push_back(counter);
        return static_cast<int>(counters_.size() - 1);
    }

    bool collect() { return query_ != nullptr && PdhCollectQueryData(query_) == ERROR_SUCCESS; }

    // All instances of a wildcard counter: (instance name, value).
    std::vector<std::pair<std::wstring, double>> values(int index) const {
        std::vector<std::pair<std::wstring, double>> result;
        if (index < 0 || static_cast<size_t>(index) >= counters_.size()) {
            return result;
        }
        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        const auto counter = counters_[static_cast<size_t>(index)];
        PDH_STATUS status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize,
                                                         &itemCount, nullptr);
        if (status != static_cast<PDH_STATUS>(PDH_MORE_DATA)) {
            return result;
        }
        buffer_.resize(bufferSize);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer_.data());
        status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize, &itemCount,
                                              items);
        if (status != ERROR_SUCCESS) {
            return result;
        }
        result.reserve(itemCount);
        for (DWORD i = 0; i < itemCount; ++i) {
            if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA ||
                items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) {
                result.emplace_back(items[i].szName, items[i].FmtValue.doubleValue);
            }
        }
        return result;
    }

private:
    PDH_HQUERY query_ = nullptr;
    std::vector<PDH_HCOUNTER> counters_;
    mutable std::vector<BYTE> buffer_;
};

}  // namespace perfpulse::sensors
