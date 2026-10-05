/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGGER_ENGINE_HPP
#define LOGGER_ENGINE_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

class ScopedEventSource {
public:
    explicit ScopedEventSource(LPCWSTR sourceName = L"CrossShellUX");
    ~ScopedEventSource();

    ScopedEventSource(const ScopedEventSource&) = delete;
    ScopedEventSource& operator=(const ScopedEventSource&) = delete;

    ScopedEventSource(ScopedEventSource&& other) noexcept;
    ScopedEventSource& operator=(ScopedEventSource&& other) noexcept;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }
    void Close();

private:
    HANDLE m_handle;
};

class EventLogEngine {
public:
    static std::wstring FormatTimestamp();
    static void EmitToEventLog(const std::wstring& message);
    static void LogLine(const std::wstring& tag, const std::wstring& message);
};

#endif // LOGGER_ENGINE_HPP
