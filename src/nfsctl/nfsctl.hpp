/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * nfsctl - NT-Native Unicode Windows NFS Control & Diagnostic Utility
 */

#ifndef NFSCTL_HPP
#define NFSCTL_HPP

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winnetwk.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <lm.h>
#include <sddl.h>

#include <stdio.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <cwctype>
#include <memory>

enum class OutputMode {
    Text,
    Json,
    Csv,
    Tsv
};

enum class NfsProbeMode {
    Both,
    V3,
    V4
};

struct GlobalOptions {
    OutputMode outputMode = OutputMode::Text;
    NfsProbeMode nfsProbeMode = NfsProbeMode::Both;
    bool dryRun = false;
    bool confirm = false;
    bool forceWrite = false;
    bool backupReg = false;
    std::wstring backupRegPath;
    bool win32Exit = false;
    bool quiet = false;
    bool verbose = false;
    bool trace = false;
    int timeoutMs = 1500;
    int opTimeoutMs = 0;
    int addressFamily = AF_INET;
};

namespace NfsRegistry {
    inline const wchar_t* kClientDefaultSubKey = L"SOFTWARE\\Microsoft\\ClientForNFS\\CurrentVersion\\Default";
    inline const wchar_t* kServerExportsSubKey = L"SOFTWARE\\Microsoft\\ServerForNFS\\CurrentVersion\\exports";
    inline const wchar_t* kValueTimeout = L"Timeout";
    inline const wchar_t* kValueRetries = L"Retries";
    inline const wchar_t* kValueMountType = L"MountType";
    inline const wchar_t* kValueDebugFlags = L"DebugFlags";
}

struct WinNfsMountInfo {
    std::wstring localDrive;
    std::wstring remotePath;
    std::wstring providerName;
};

struct NfsIdMapResult {
    DWORD code = ERROR_SUCCESS;
    std::wstring account;
    std::wstring domain;
    std::wstring sid;
};

class WinsockSubsystem {
public:
    WinsockSubsystem() : m_status(0), m_initialized(false) {
        WSADATA wsa;
        m_status = WSAStartup(MAKEWORD(2, 2), &wsa);
        m_initialized = (m_status == 0);
    }

    ~WinsockSubsystem() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool Ok() const { return m_initialized; }
    int Status() const { return m_status; }

private:
    int m_status;
    bool m_initialized;
};

class ScopedRegistryKey {
public:
    explicit ScopedRegistryKey(HKEY hKey = NULL) : m_hKey(hKey) {}

    ~ScopedRegistryKey() {
        Close();
    }

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    ScopedRegistryKey(ScopedRegistryKey&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = NULL;
    }

    ScopedRegistryKey& operator=(ScopedRegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_hKey = other.m_hKey;
            other.m_hKey = NULL;
        }
        return *this;
    }

    HKEY Get() const { return m_hKey; }
    HKEY* Receive() { Close(); return &m_hKey; }
    bool IsValid() const { return m_hKey != NULL; }

    void Close() {
        if (m_hKey != NULL) {
            RegCloseKey(m_hKey);
            m_hKey = NULL;
        }
    }

private:
    HKEY m_hKey;
};

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedServiceHandle() {
        Close();
    }

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    SC_HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != NULL; }

    void Close() {
        if (m_handle != NULL) {
            CloseServiceHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    SC_HANDLE m_handle;
};

#endif // NFSCTL_HPP
