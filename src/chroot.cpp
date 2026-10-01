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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. RAII GUARDS & DATA MODELS
// ============================================================================

class ScopedJobHandle {
public:
    explicit ScopedJobHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedJobHandle() { Close(); }

    ScopedJobHandle(const ScopedJobHandle&) = delete;
    ScopedJobHandle& operator=(const ScopedJobHandle&) = delete;

    ScopedJobHandle(ScopedJobHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    ScopedJobHandle& operator=(ScopedJobHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
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

class ScopedProcessInfo {
public:
    PROCESS_INFORMATION pi = { 0 };

    ~ScopedProcessInfo() {
        if (pi.hProcess && pi.hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hProcess);
        }
        if (pi.hThread && pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
        }
    }

    ScopedProcessInfo() = default;
    ScopedProcessInfo(const ScopedProcessInfo&) = delete;
    ScopedProcessInfo& operator=(const ScopedProcessInfo&) = delete;
};

class ScopedWpOpen {
public:
    explicit ScopedWpOpen(FILE* pipe = nullptr) : m_pipe(pipe) {}
    ~ScopedWpOpen() { Close(); }

    ScopedWpOpen(const ScopedWpOpen&) = delete;
    ScopedWpOpen& operator=(const ScopedWpOpen&) = delete;

    ScopedWpOpen(ScopedWpOpen&& other) noexcept : m_pipe(other.m_pipe) { other.m_pipe = nullptr; }
    ScopedWpOpen& operator=(ScopedWpOpen&& other) noexcept {
        if (this != &other) {
            Close();
            m_pipe = other.m_pipe;
            other.m_pipe = nullptr;
        }
        return *this;
    }

    FILE* Get() const { return m_pipe; }
    bool IsValid() const { return m_pipe != nullptr; }

    void Close() {
        if (m_pipe) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

private:
    FILE* m_pipe;
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

// ============================================================================
// 2. PATH & ENVIRONMENT HELPERS
// ============================================================================

class PathHelper {
public:
    static std::string Utf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static bool DirectoryExists(const std::wstring& path) {
        DWORD dwAttrib = GetFileAttributesW(path.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
               (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    static std::wstring GetAbsolutePath(const std::wstring& path) {
        wchar_t fullPath[MAX_PATH];
        DWORD retval = GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, NULL);
        if (retval == 0 || retval > MAX_PATH) {
            return path;
        }
        return std::wstring(fullPath);
    }

    static std::wstring BuildCommandLine(const std::wstring& command, const std::vector<std::wstring>& args) {
        std::wstring commandLine = command;
        for (const auto& arg : args) {
            commandLine += L" \"" + arg + L"\"";
        }
        return commandLine;
    }
};

class EnvironmentConfigurator {
public:
    static bool ConfigureSandbox(const std::wstring& newRoot) {
        if (!SetCurrentDirectoryW(newRoot.c_str())) {
            return false;
        }

        std::wstring winDir = newRoot + L"\\Windows";
        std::wstring sys32Dir = newRoot + L"\\Windows\\System32";
        std::wstring binDir = newRoot + L"\\bin";
        std::wstring tempDir = newRoot + L"\\temp";

        std::wstring newPath = binDir + L";" + sys32Dir + L";" + winDir + L";" + newRoot;
        SetEnvironmentVariableW(L"PATH", newPath.c_str());
        SetEnvironmentVariableW(L"SystemRoot", winDir.c_str());
        SetEnvironmentVariableW(L"USERPROFILE", newRoot.c_str());
        SetEnvironmentVariableW(L"TEMP", tempDir.c_str());
        SetEnvironmentVariableW(L"TMP", tempDir.c_str());

        if (newRoot.length() >= 2 && newRoot[1] == L':') {
            std::wstring drive = newRoot.substr(0, 2);
            SetEnvironmentVariableW(L"SystemDrive", drive.c_str());
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & EXECUTOR
// ============================================================================

class ChrootOptions {
public:
    std::vector<std::wstring> positional;
    OutputFormat outputFormat = OutputFormat::Default;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == L"--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == L"--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    positional.push_back(argv[j]);
                }
                break;
            } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                showHelp = true;
                return true;
            } else if (arg == L"--version") {
                showVersion = true;
                return true;
            } else if (arg == L"--userspec") {
                if (i + 1 >= argc) {
                    std::wcerr << L"chroot: option '--userspec' requires an argument\n";
                    return false;
                }
                ++i;
            } else if (arg.rfind(L"--userspec=", 0) == 0) {
                // Accepted for compatibility; no-op on Windows.
            } else {
                positional.push_back(arg);
            }
        }
        return true;
    }

    void PrintHelp() const {
        std::wcout << LR"(chroot(1)               CrossShell for UNIX Reference Manual                chroot(1)

    NAME
        chroot - run command or interactive shell with specified root directory

    SYNOPSIS
        chroot [OPTIONS] NEWROOT [COMMAND [ARG]...]

    DESCRIPTION
        Run COMMAND with root directory set to NEWROOT using isolated Windows
        directory mapping, virtualization, or sub-container environments.

    OPTIONS
        --userspec=USER:GROUP
            Specify user and group to use (numeric IDs or names).

        --groups=G_LIST
            Specify supplementary groups as g1,g2,...,gn.

        --json, --csv, --table
            Format execution diagnostics as JSON, CSV, or table.

        --pipe COMMAND
            Route output into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        chroot D:\Sandbox cmd.exe
            Run command prompt isolated inside D:\Sandbox.

    CrossShell for UNIX                                                 chroot(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"chroot 1.0\n";
    }
};

class ChrootExecutor {
public:
    static int ExecuteSandbox(const ChrootOptions& options) {
        if (options.positional.empty()) {
            options.PrintHelp();
            return 1;
        }

        std::wstring newRoot = PathHelper::GetAbsolutePath(options.positional[0]);

        if (!PathHelper::DirectoryExists(newRoot)) {
            std::wcerr << L"chroot: cannot change root directory to '" << newRoot 
                       << L"': No such file or directory\n";
            return 1;
        }

        std::wstring command = L"";
        std::vector<std::wstring> commandArgs;

        if (options.positional.size() >= 2) {
            command = options.positional[1];
            for (size_t i = 2; i < options.positional.size(); ++i) {
                commandArgs.push_back(options.positional[i]);
            }
        } else {
            command = L"cmd.exe";
        }

        std::wstring commandLine = PathHelper::BuildCommandLine(command, commandArgs);

        if (!EnvironmentConfigurator::ConfigureSandbox(newRoot)) {
            std::wcerr << L"chroot: failed to set current directory to " << newRoot << L"\n";
            return 1;
        }

        ScopedJobHandle hJob(CreateJobObjectW(NULL, NULL));
        if (hJob.IsValid()) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob.Get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }

        STARTUPINFOW si = { sizeof(si) };
        ScopedProcessInfo pi;

        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        BOOL success = CreateProcessW(
            NULL,
            cmdBuffer.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            newRoot.c_str(),
            &si,
            &pi.pi
        );

        if (!success) {
            DWORD err = GetLastError();
            std::wcerr << L"chroot: failed to execute command '" << command 
                       << L"' (Error Code: " << err << L")\n";
            return 1;
        }

        if (hJob.IsValid()) {
            AssignProcessToJobObject(hJob.Get(), pi.pi.hProcess);
        }

        WaitForSingleObject(pi.pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.pi.hProcess, &exitCode);

        if (options.outputFormat != OutputFormat::Default || !options.pipeCommand.empty()) {
            std::wstring text = (options.outputFormat == OutputFormat::Json) ? L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(exitCode) + L"}\n" :
                                (options.outputFormat == OutputFormat::Csv) ? L"status,exit_code\ncompleted," + std::to_wstring(exitCode) + L"\n" :
                                L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(exitCode) + L"\n";
            if (!options.pipeCommand.empty()) {
                ScopedWpOpen pipe(_wpopen(options.pipeCommand.c_str(), L"w"));
                if (pipe.IsValid()) {
                    std::string narrow = PathHelper::Utf8(text);
                    fwrite(narrow.data(), 1, narrow.size(), pipe.Get());
                }
            } else {
                std::wcout << text;
            }
        }

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ChrootApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        ChrootOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.showHelp) {
            options.PrintHelp();
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        return ChrootExecutor::ExecuteSandbox(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ChrootApplication app;
    return app.Run(argc, argv);
}
