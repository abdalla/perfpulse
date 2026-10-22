#include "sensors/lhm_source.hpp"

#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>

#include "core/log.hpp"

namespace perfpulse::sensors {

namespace {

// Small RAII holder for COM pointers returned by WMI.
template <typename T>
class ComRef {
public:
    ComRef() = default;
    ~ComRef() {
        if (ptr_ != nullptr) {
            ptr_->Release();
        }
    }
    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;
    T** put() { return &ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

private:
    T* ptr_ = nullptr;
};

std::wstring stringProperty(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR) {
        result = value.bstrVal;
    }
    VariantClear(&value);
    return result;
}

float floatProperty(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    float result = session::kNoValue;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr))) {
        if (value.vt == VT_R4) {
            result = value.fltVal;
        } else if (value.vt == VT_R8) {
            result = static_cast<float>(value.dblVal);
        }
    }
    VariantClear(&value);
    return result;
}

}  // namespace

std::unique_ptr<LhmSource> LhmSource::create() {
    std::unique_ptr<LhmSource> source(new LhmSource());
    if (!source->connect() || !source->locateSensors()) {
        return nullptr;
    }
    core::log::info("LibreHardwareMonitor WMI found");
    return source;
}

LhmSource::~LhmSource() {
    if (services_ != nullptr) {
        services_->Release();
    }
}

bool LhmSource::connect() {
    ComRef<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                reinterpret_cast<void**>(locator.put())))) {
        return false;
    }
    const _bstr_t ns(L"ROOT\\LibreHardwareMonitor");
    if (FAILED(locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services_))) {
        services_ = nullptr;
        return false;
    }
    CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return true;
}

bool LhmSource::locateSensors() {
    ComRef<IEnumWbemClassObject> enumerator;
    const _bstr_t language(L"WQL");
    const _bstr_t query(
        L"SELECT Identifier, Name, SensorType FROM Sensor WHERE SensorType='Temperature' OR SensorType='Power'");
    if (FAILED(services_->ExecQuery(language, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                    nullptr, enumerator.put()))) {
        return false;
    }
    while (true) {
        ComRef<IWbemClassObject> object;
        ULONG returned = 0;
        if (enumerator->Next(2000, 1, object.put(), &returned) != WBEM_S_NO_ERROR || returned == 0) {
            break;
        }
        const auto identifier = stringProperty(object.get(), L"Identifier");
        if (identifier.find(L"cpu/") == std::wstring::npos) {
            continue;
        }
        const auto name = stringProperty(object.get(), L"Name");
        const auto type = stringProperty(object.get(), L"SensorType");
        if (type == L"Temperature" && temperatureId_.empty() &&
            (name == L"CPU Package" || name == L"Core (Tctl/Tdie)" || name == L"Package")) {
            temperatureId_ = identifier;
        } else if (type == L"Power" && powerId_.empty() && (name == L"CPU Package" || name == L"Package")) {
            powerId_ = identifier;
        }
    }
    return !temperatureId_.empty() || !powerId_.empty();
}

void LhmSource::read(session::SensorSample& sample) {
    std::wstring query = L"SELECT Identifier, Value FROM Sensor WHERE Identifier='" + temperatureId_ +
                         L"' OR Identifier='" + powerId_ + L"'";
    ComRef<IEnumWbemClassObject> enumerator;
    if (FAILED(services_->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query.c_str()),
                                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                                    enumerator.put()))) {
        return;
    }
    while (true) {
        ComRef<IWbemClassObject> object;
        ULONG returned = 0;
        if (enumerator->Next(500, 1, object.put(), &returned) != WBEM_S_NO_ERROR || returned == 0) {
            break;
        }
        const auto identifier = stringProperty(object.get(), L"Identifier");
        const float value = floatProperty(object.get(), L"Value");
        if (identifier == temperatureId_) {
            sample.cpuTemperature = value;
        } else if (identifier == powerId_) {
            sample.cpuPower = value;
        }
    }
}

}  // namespace perfpulse::sensors
