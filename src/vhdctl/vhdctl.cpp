/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <initguid.h>
#include <virtdisk.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <limits>
#include <cstdio>
#include <streambuf>
#include <memory>

#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. RAII GUARDS & DATA MODELS
// ============================================================================

enum class OutputFormat { Human, Json, Csv, Table };

class ScopedVirtualDiskHandle {
public:
    explicit ScopedVirtualDiskHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedVirtualDiskHandle() { Close(); }

    ScopedVirtualDiskHandle(const ScopedVirtualDiskHandle&) = delete;
    ScopedVirtualDiskHandle& operator=(const ScopedVirtualDiskHandle&) = delete;

    ScopedVirtualDiskHandle(ScopedVirtualDiskHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    ScopedVirtualDiskHandle& operator=(ScopedVirtualDiskHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* AddressOf() { return &m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedProcessToken {
public:
    explicit ScopedProcessToken(HANDLE token = nullptr) : m_token(token) {}
    ~ScopedProcessToken() { Close(); }

    ScopedProcessToken(const ScopedProcessToken&) = delete;
    ScopedProcessToken& operator=(const ScopedProcessToken&) = delete;

    HANDLE Get() const { return m_token; }
    bool IsValid() const { return m_token != nullptr && m_token != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_token);
            m_token = nullptr;
        }
    }

private:
    HANDLE m_token;
};

class ScopedSid {
public:
    explicit ScopedSid(PSID sid = nullptr) : m_sid(sid) {}
    ~ScopedSid() { Close(); }

    ScopedSid(const ScopedSid&) = delete;
    ScopedSid& operator=(const ScopedSid&) = delete;

    PSID Get() const { return m_sid; }
    bool IsValid() const { return m_sid != nullptr; }

    void Close() {
        if (IsValid()) {
            FreeSid(m_sid);
            m_sid = nullptr;
        }
    }

private:
    PSID m_sid;
};

class WidePipeBuffer : public std::wstreambuf {
    FILE* pipe_;
    wchar_t buffer_[2048];
protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) { *pptr() = static_cast<wchar_t>(ch); pbump(1); }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }
    int sync() override {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), pipe_) != static_cast<size_t>(count)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_) / sizeof(buffer_[0]));
        return std::fflush(pipe_) == 0 ? 0 : -1;
    }
public:
    explicit WidePipeBuffer(FILE* pipe) : pipe_(pipe) { setp(buffer_, buffer_ + sizeof(buffer_) / sizeof(buffer_[0])); }
    ~WidePipeBuffer() override { sync(); }
};

class ScopedWidePipeRedirect {
public:
    explicit ScopedWidePipeRedirect(const std::wstring& pipeCommand) {
        if (!pipeCommand.empty()) {
            m_pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (m_pipe) {
                m_oldOutput = std::wcout.rdbuf();
                m_buffer = std::make_unique<WidePipeBuffer>(m_pipe);
                std::wcout.rdbuf(m_buffer.get());
            }
        }
    }

    ~ScopedWidePipeRedirect() {
        if (m_pipe) {
            std::wcout.rdbuf(m_oldOutput);
            m_buffer.reset();
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

    bool IsValid() const { return m_pipe != nullptr; }

private:
    FILE* m_pipe = nullptr;
    std::wstreambuf* m_oldOutput = nullptr;
    std::unique_ptr<WidePipeBuffer> m_buffer;
};

// ============================================================================
// 2. STRING & INSPECTION HELPERS
// ============================================================================

class VhdStorageInspector {
public:
    static std::wstring CsvQuote(const std::wstring& value) {
        std::wstring result = L"\"";
        for (wchar_t ch : value) { if (ch == L'"') result += L"\"\""; else result += ch; }
        return result + L"\"";
    }

    static std::wstring GetErrorMessage(DWORD errorCode) {
        LPWSTR buffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buffer, 0, NULL);
        
        if (size == 0 || !buffer) {
            return L"Unknown error code " + std::to_wstring(errorCode);
        }

        std::wstring message(buffer, size);
        LocalFree(buffer);
        
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L' ')) {
            message.pop_back();
        }
        return message;
    }

    static bool IsAdmin() {
        HANDLE token = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            ScopedProcessToken scopedToken(token);
            TOKEN_ELEVATION elevation = {};
            DWORD size = 0;
            if (GetTokenInformation(scopedToken.Get(), TokenElevation, &elevation, sizeof(elevation), &size)) {
                return elevation.TokenIsElevated != 0;
            }
        }

        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            ScopedSid scopedSid(adminGroup);
            CheckTokenMembership(NULL, scopedSid.Get(), &isAdmin);
        }
        return isAdmin == TRUE;
    }

    static ULONG GetDeviceType(const std::wstring& path) {
        if (path.empty()) return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos || dot + 1 >= path.size()) return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
        std::wstring ext = path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext == L".vhdx") return VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
        if (ext == L".vhd") return VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
        return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    }

    static ULONGLONG ParseSize(const std::wstring& str) {
        if (str.empty()) return 0;

        std::wstring trimmed = str;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](wchar_t ch) {
            return !std::iswspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](wchar_t ch) {
            return !std::iswspace(ch);
        }).base(), trimmed.end());

        if (trimmed.empty()) return 0;

        wchar_t unit = std::towupper(trimmed.back());
        ULONGLONG multiplier = 1;
        std::wstring numStr = trimmed;

        if (unit == L'K') { multiplier = 1024ULL; numStr.pop_back(); }
        else if (unit == L'M') { multiplier = 1024ULL * 1024ULL; numStr.pop_back(); }
        else if (unit == L'G') { multiplier = 1024ULL * 1024ULL * 1024ULL; numStr.pop_back(); }
        else if (unit == L'T') { multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL; numStr.pop_back(); }

        if (numStr.empty()) return 0;

        try {
            unsigned long long value = std::stoull(numStr);
            const unsigned long long maxValue = ~0ULL;
            if (value > (maxValue / multiplier)) return 0;
            return value * multiplier;
        } catch (...) {
            return 0;
        }
    }
};

// ============================================================================
// 3. VIRTUAL DISK OPERATIONS
// ============================================================================

class VhdReporter {
public:
    static void EmitResult(OutputFormat format, const std::wstring& action, const std::wstring& path, const std::wstring& detail) {
        if (format == OutputFormat::Json) {
            std::wcout << L"{\"action\":\"" << action << L"\",\"path\":\"" << path << L"\",\"detail\":\"" << detail << L"\"}\n";
        } else if (format == OutputFormat::Csv) {
            std::wcout << VhdStorageInspector::CsvQuote(action) << L"," << VhdStorageInspector::CsvQuote(path) << L"," << VhdStorageInspector::CsvQuote(detail) << L"\n";
        } else if (format == OutputFormat::Table) {
            std::wcout << L"ACTION\tPATH\tDETAIL\n" << action << L"\t" << path << L"\t" << detail << L"\n";
        } else {
            std::wcout << L"vhdctl: " << detail << L": " << path << L"\n";
        }
    }

    static void PrintUsage() {
        std::wcout << LR"(vhdctl(1)               CrossShell for UNIX Reference Manual                 vhdctl(1)

    NAME
        vhdctl - create, mount, and manage VHD and VHDX virtual disk images

    SYNOPSIS
        vhdctl COMMAND [OPTIONS]

    DESCRIPTION
        Creates, mounts, and unmounts Virtual Hard Disk (VHD and VHDX) container
        files using the Windows VirtDisk API subsystem. Requires administrative
        privileges to attach and detach disk images in the Windows storage stack.

    COMMANDS
        create, cr
            Create a new fixed or dynamically expanding virtual disk file.

        mount, attach
            Attach a virtual disk image to the system storage hierarchy.

        unmount, detach
            Detach an active virtual disk image from the system.

    OPTIONS
        -f, --file PATH
            Specify the target .vhd or .vhdx disk image file path.

        -s, --size SIZE
            Specify the maximum virtual disk capacity (e.g., 500M, 10G, 1T).
            Required for create.

        -t, --type TYPE
            Set allocation type: dynamic (sparse/expanding) or fixed
            (fully allocated). Default is dynamic.

        -r, --readonly
            Attach the virtual disk in read-only mode (mount only).

        --json, --csv, --table
            Format execution summary output as JSON, CSV, or an aligned table.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        vhdctl create -f C:\Disks\storage.vhdx -s 50G -t dynamic
            Create a 50 GB dynamically expanding VHDX disk image.

        vhdctl mount -f C:\Disks\storage.vhdx
            Attach a virtual disk with read-write access.

        vhdctl mount -f C:\Disks\backup.vhd -r
            Mount a virtual disk in read-only mode.

        vhdctl unmount -f C:\Disks\storage.vhdx
            Detach the virtual disk from the system.

        vhdctl create -f D:\data.vhdx -s 100G --json
            Create a virtual disk and emit execution details as JSON.

    CrossShell for UNIX                                                     vhdctl(1)
)";
    }
    static void PrintVersion() {
        std::wcout << L"vhdctl v1.0.0\n";
    }
};

class VhdOperations {
public:
    static bool CreateDisk(const std::wstring& path, ULONGLONG sizeInBytes, bool isFixed, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        if (storageType.DeviceId == VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN) {
            storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
        }
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        CREATE_VIRTUAL_DISK_PARAMETERS params = {};
        params.Version = CREATE_VIRTUAL_DISK_VERSION_2;
        params.Version2.UniqueId = GUID{};
        params.Version2.MaximumSize = sizeInBytes;
        params.Version2.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE;
        params.Version2.SectorSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_SECTOR_SIZE;
        params.Version2.ParentPath = NULL;
        params.Version2.SourcePath = NULL;

        CREATE_VIRTUAL_DISK_FLAG flags = CREATE_VIRTUAL_DISK_FLAG_NONE;
        if (isFixed) {
            flags |= CREATE_VIRTUAL_DISK_FLAG_FULL_PHYSICAL_ALLOCATION;
        }

        ScopedVirtualDiskHandle hVhd;
        if (format == OutputFormat::Human) {
            std::wcout << L"vhdctl: Creating " << (isFixed ? L"fixed" : L"dynamic")
                       << L" virtual disk (" << (sizeInBytes / (1024 * 1024)) << L" MB)..." << std::endl;
        }

        DWORD result = CreateVirtualDisk(
            &storageType,
            path.c_str(),
            VIRTUAL_DISK_ACCESS_NONE,
            NULL,
            flags,
            0,
            &params,
            NULL,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error creating disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"create", path, L"Virtual disk created successfully");
        return true;
    }

    static bool MountDisk(const std::wstring& path, bool readOnly, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
        openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;

        VIRTUAL_DISK_ACCESS_MASK accessMask = VIRTUAL_DISK_ACCESS_ATTACH_RO | VIRTUAL_DISK_ACCESS_GET_INFO;
        if (!readOnly) {
            accessMask |= VIRTUAL_DISK_ACCESS_ATTACH_RW;
        }

        ScopedVirtualDiskHandle hVhd;
        DWORD result = OpenVirtualDisk(
            &storageType,
            path.c_str(),
            accessMask,
            OPEN_VIRTUAL_DISK_FLAG_NONE,
            &openParams,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error opening disk file: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        ATTACH_VIRTUAL_DISK_PARAMETERS attachParams = {};
        attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

        ATTACH_VIRTUAL_DISK_FLAG attachFlags = ATTACH_VIRTUAL_DISK_FLAG_NONE;
        if (readOnly) {
            attachFlags |= ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY;
        }

        result = AttachVirtualDisk(
            hVhd.Get(),
            NULL,
            attachFlags,
            0,
            &attachParams,
            NULL
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error mounting disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"mount", path, readOnly ? L"Virtual disk mounted successfully [Read-Only]" : L"Virtual disk mounted successfully [Read-Write]");
        return true;
    }

    static bool UnmountDisk(const std::wstring& path, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
        openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;

        ScopedVirtualDiskHandle hVhd;
        DWORD result = OpenVirtualDisk(
            &storageType,
            path.c_str(),
            VIRTUAL_DISK_ACCESS_DETACH,
            OPEN_VIRTUAL_DISK_FLAG_NONE,
            &openParams,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error opening mounted disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        result = DetachVirtualDisk(hVhd.Get(), DETACH_VIRTUAL_DISK_FLAG_NONE, 0);

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error unmounting disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"unmount", path, L"Virtual disk unmounted successfully");
        return true;
    }
};

// ============================================================================
// 4. OPTIONS & APPLICATION CONTROLLER
// ============================================================================

class VhdctlOptions {
public:
    std::wstring command;
    std::wstring filePath;
    std::wstring sizeStr;
    bool isFixed = false;
    bool readOnly = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return false;
        }

        command = argv[1];
        if (command == L"-h" || command == L"--help" || command == L"help") {
            showHelp = true;
            return true;
        }
        if (command == L"--version") {
            showVersion = true;
            return true;
        }

        for (int i = 2; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--") {
                break;
            } else if ((arg == L"-f" || arg == L"--file") && i + 1 < argc) {
                filePath = argv[++i];
            } else if ((arg == L"-s" || arg == L"--size") && i + 1 < argc) {
                sizeStr = argv[++i];
            } else if ((arg == L"-t" || arg == L"--type") && i + 1 < argc) {
                std::wstring type = argv[++i];
                if (type == L"fixed") isFixed = true;
                else if (type == L"dynamic") isFixed = false;
                else {
                    std::wcerr << L"vhdctl: invalid type '" << type << L"'" << std::endl;
                    return false;
                }
            } else if (arg == L"-r" || arg == L"--readonly") {
                readOnly = true;
            } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                outputFormat = (arg == L"--json") ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
            } else if (arg == L"--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg[0] == L'-') {
                std::wcerr << L"vhdctl: invalid option '" << arg << L"'" << std::endl;
                return false;
            } else {
                std::wcerr << L"vhdctl: unexpected argument '" << arg << L"'" << std::endl;
                return false;
            }
        }

        if (filePath.empty()) {
            std::wcerr << L"vhdctl: Missing required parameter -f / --file." << std::endl;
            return false;
        }

        return true;
    }
};

class VhdctlApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        VhdctlOptions opts;
        if (!opts.Parse(argc, argv)) {
            if (opts.showHelp) {
                VhdReporter::PrintUsage();
                return 0;
            }
            if (opts.showVersion) {
                VhdReporter::PrintVersion();
                return 0;
            }
            return 1;
        }

        if (opts.showHelp) {
            VhdReporter::PrintUsage();
            return 0;
        }
        if (opts.showVersion) {
            VhdReporter::PrintVersion();
            return 0;
        }

        if (!VhdStorageInspector::IsAdmin()) {
            std::wcerr << L"vhdctl: Access denied. Run this command from an elevated (Run as administrator) terminal." << std::endl;
            return ERROR_ACCESS_DENIED;
        }

        ScopedWidePipeRedirect pipeRedirect(opts.pipeCommand);
        if (!opts.pipeCommand.empty() && !pipeRedirect.IsValid()) {
            std::wcerr << L"vhdctl: failed to start pipe command.\n";
            return 1;
        }

        if (opts.command == L"create" || opts.command == L"cr") {
            if (opts.sizeStr.empty()) {
                std::wcerr << L"vhdctl: Missing required parameter -s / --size for create command." << std::endl;
                return 1;
            }
            ULONGLONG sizeBytes = VhdStorageInspector::ParseSize(opts.sizeStr);
            if (sizeBytes == 0) {
                std::wcerr << L"vhdctl: Invalid size specifier '" << opts.sizeStr << L"'." << std::endl;
                return 1;
            }
            return VhdOperations::CreateDisk(opts.filePath, sizeBytes, opts.isFixed, opts.outputFormat) ? 0 : 1;

        } else if (opts.command == L"mount" || opts.command == L"attach") {
            return VhdOperations::MountDisk(opts.filePath, opts.readOnly, opts.outputFormat) ? 0 : 1;

        } else if (opts.command == L"unmount" || opts.command == L"detach") {
            return VhdOperations::UnmountDisk(opts.filePath, opts.outputFormat) ? 0 : 1;

        } else {
            std::wcerr << L"vhdctl: Unknown command '" << opts.command << L"'." << std::endl;
            VhdReporter::PrintUsage();
            return 1;
        }
    }
};

int wmain(int argc, wchar_t* argv[]) {
    VhdctlApplication app;
    return app.Run(argc, argv);
}
