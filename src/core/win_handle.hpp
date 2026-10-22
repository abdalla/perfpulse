#pragma once

#include <windows.h>

#include <utility>

namespace perfpulse::core {

// RAII owner for kernel HANDLEs closed with CloseHandle.
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.handle_, nullptr));
        }
        return *this;
    }

    HANDLE get() const { return handle_; }
    bool valid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
    explicit operator bool() const { return valid(); }

    void reset(HANDLE handle = nullptr) {
        if (valid()) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

// RAII owner for an HMODULE loaded with LoadLibrary.
class UniqueModule {
public:
    UniqueModule() = default;
    explicit UniqueModule(HMODULE module) : module_(module) {}
    ~UniqueModule() { reset(); }

    UniqueModule(const UniqueModule&) = delete;
    UniqueModule& operator=(const UniqueModule&) = delete;
    UniqueModule(UniqueModule&& other) noexcept : module_(std::exchange(other.module_, nullptr)) {}
    UniqueModule& operator=(UniqueModule&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.module_, nullptr));
        }
        return *this;
    }

    HMODULE get() const { return module_; }
    explicit operator bool() const { return module_ != nullptr; }

    template <typename Fn>
    Fn proc(const char* name) const {
        return module_ ? reinterpret_cast<Fn>(GetProcAddress(module_, name)) : nullptr;
    }

    void reset(HMODULE module = nullptr) {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
        module_ = module;
    }

private:
    HMODULE module_ = nullptr;
};

}  // namespace perfpulse::core
