#ifndef NOHUP_HPP
#define NOHUP_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedFileHandle() {
        Close();
    }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    void Reset(HANDLE handle = INVALID_HANDLE_VALUE) {
        Close();
        m_handle = handle;
    }

private:
    HANDLE m_handle;
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

class NohupPathResolver {
public:
    static bool IsConsoleHandle(HANDLE h);
    static HANDLE SafeDuplicateHandle(HANDLE hSource, HANDLE hFallback);
    static std::wstring GetNohupOutputPath();
    static bool EndsWithIcase(const std::wstring& str, const std::wstring& suffix);
    static std::wstring QuoteArgument(const std::wstring& arg);
};

#endif // NOHUP_HPP
