/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "engine.hpp"
#include <iostream>

ScopedEventSource::ScopedEventSource(LPCWSTR sourceName)
    : m_handle(RegisterEventSourceW(nullptr, sourceName)) {}

ScopedEventSource::~ScopedEventSource() {
    Close();
}

ScopedEventSource::ScopedEventSource(ScopedEventSource&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

ScopedEventSource& ScopedEventSource::operator=(ScopedEventSource&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void ScopedEventSource::Close() {
    if (m_handle) {
        DeregisterEventSource(m_handle);
        m_handle = nullptr;
    }
}

std::wstring EventLogEngine::FormatTimestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

void EventLogEngine::EmitToEventLog(const std::wstring& message) {
    ScopedEventSource source(L"CrossShellUX");
    if (!source.IsValid()) {
        return;
    }

    LPCWSTR strings[] = { message.c_str() };
    ReportEventW(source.Get(), EVENTLOG_INFORMATION_TYPE, 0, 0x1000, nullptr, 1, 0, strings, nullptr);
}

void EventLogEngine::LogLine(const std::wstring& tag, const std::wstring& message) {
    std::wstring formatted = FormatTimestamp() + L" " + tag + L": " + message;
    std::wcout << formatted << L"\n";
    EmitToEventLog(formatted);
}
