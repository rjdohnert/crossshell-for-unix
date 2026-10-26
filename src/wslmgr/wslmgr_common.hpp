#pragma once
/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024-2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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

// Single-file index:
//   1. Feature flags, standard library includes, and color definitions
//   2. Shared data types: ProcessResult, DistroInfo, OnlineDistro, WslProcess
//   3. ConsoleGuard: console modes, alternate screen, synchronized redraw,
//      cleanup, buffer handling, and signal restoration
//   4. Sys: process execution, encoding, terminal launch, jobs, timeouts,
//      filesystem path conversion, and spinners
//   5. WSL: distro discovery, online catalog, process listing, and process
//      termination commands
//   6. App input: keyboard, F8/F9 shortcuts, mouse clicks, and mouse wheel
//   7. App layout: responsive console sizing, tab hitboxes, and output cleanup
//   8. App views: Explorer, Catalog, Import, Diagnostics, Lifecycle, and Help
//   9. App workflows: distro actions, scrollable process/Help viewports,
//      verbose Lifecycle reporting, and import/export operations
//  10. main: application entry point

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <future>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <cstring>

namespace fs = std::filesystem;

namespace Color {
    inline std::string RESET        = "\033[0m";
    inline std::string BOLD         = "\033[1m";
    inline std::string RED          = "\033[91m";
    inline std::string GREEN        = "\033[92m";
    inline std::string YELLOW       = "\033[93m";
    inline std::string BLUE         = "\033[94m";
    inline std::string MAGENTA      = "\033[95m";
    inline std::string CYAN         = "\033[96m";
    inline std::string WHITE        = "\033[97m";
    inline std::string HEADER_TEXT  = "\033[97m";
    inline std::string BG_DARK      = "\033[48;5;235m";
    inline std::string BG_BLUE      = "\033[44m";
    inline std::string BG_HIGHLIGHT = "\033[48;5;238m";
    inline std::string CLEAR_LINE   = "\033[K";
}

enum class ThemeName {
    Silver,
    Blue,
    Orange,
    Black
};

enum class GlyphStyle {
    Ascii,
    Unicode,
    NerdFont
};

struct ThemePalette {
    std::string headerBg;
    std::string headerText;
    std::string tabBg;
    std::string tabSelected;
    std::string accent;
    std::string success;
    std::string warning;
    std::string danger;
    std::string muted;
    std::string highlight;
};

namespace Glyph {
    inline std::string check = "✓";
    inline std::string cross = "✗";
    inline std::string dot = "•";
    inline std::string arrow = "→";
    inline std::string active = "▶";
    inline std::string menu = "▣";
    inline std::string warn = "⚠";
    inline std::string theme = "◐";
}



struct DistroInfo;
struct OnlineDistro;
struct WslProcess;
class ConsoleGuard;
class Sys;
class WSL;
class App;
enum class InputKey;
struct InputEvent;
ThemePalette MakePalette(ThemeName theme);
void ApplyTheme(ThemeName theme);
void ApplyGlyphs(GlyphStyle style);
int main();

struct ProcessResult {
    int exitCode = -1;
    std::string stdOut;
    std::string stdErr;
    bool success() const { return exitCode == 0; }
};

struct DistroInfo {
    bool isDefault = false;
    std::string name;
    std::string state;
    std::string version;
};

struct OnlineDistro {
    std::string id;
    std::string friendlyName;
};

struct WslProcess {
    unsigned long pid = 0;
    std::string name;
    std::string command;
};

struct WslContainer {
    std::string id;
};

// -----------------------------------------------------------------------------
// RAII Terminal Guard: Alternate Buffer, State Restoration & Signal Handling
// -----------------------------------------------------------------------------

class ConsoleGuard {
private:
    DWORD origOutMode = 0;
    DWORD origInMode = 0;
    UINT origOutCP = 0;
    UINT origInCP = 0;
    HANDLE hOut = INVALID_HANDLE_VALUE;
    HANDLE hIn = INVALID_HANDLE_VALUE;
    bool initialized = false;

    static ConsoleGuard* instance;

    void ClearBuffer() {
        if (hOut == INVALID_HANDLE_VALUE) return;

        CONSOLE_SCREEN_BUFFER_INFO info = {};
        if (!GetConsoleScreenBufferInfo(hOut, &info)) return;

        const COORD origin = { 0, 0 };
        const DWORD cellCount = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        DWORD written = 0;
        FillConsoleOutputCharacterW(hOut, L' ', cellCount, origin, &written);
        FillConsoleOutputAttribute(hOut, info.wAttributes, cellCount, origin, &written);
        SetConsoleCursorPosition(hOut, origin);
    }

    static BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
            if (instance) {
                instance->Restore();
            }
            return FALSE;
        }
        return FALSE;
    }

public:
    ConsoleGuard() {
        instance = this;
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hIn = GetStdHandle(STD_INPUT_HANDLE);

        if (hOut != INVALID_HANDLE_VALUE && hIn != INVALID_HANDLE_VALUE &&
            GetConsoleMode(hOut, &origOutMode) && GetConsoleMode(hIn, &origInMode)) {
            origOutCP = GetConsoleOutputCP();
            origInCP = GetConsoleCP();

            const DWORD outMode = origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            const DWORD inMode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT;
            if (!SetConsoleMode(hOut, outMode) || !SetConsoleMode(hIn, inMode) ||
                !SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8)) {
                SetConsoleMode(hOut, origOutMode);
                SetConsoleMode(hIn, origInMode);
                if (origOutCP != 0) SetConsoleOutputCP(origOutCP);
                if (origInCP != 0) SetConsoleCP(origInCP);
                return;
            }

            SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
            initialized = true;
            FlushConsoleInputBuffer(hIn);

            // Switch to Alternate Screen Buffer & hide cursor
            std::cout << "\033[?1049h\033[?25l" << std::flush;
            ClearBuffer();
        }
    }

    bool IsReady() const { return initialized; }

    void Restore() {
        if (initialized) {
            // Restore Main Buffer, cursor visibility, and color styling
            std::cout << "\033[?25h\033[0m\033[?1049l" << std::flush;
            if (hOut != INVALID_HANDLE_VALUE) {
                SetConsoleMode(hOut, origOutMode);
                SetConsoleOutputCP(origOutCP);
            }
            if (hIn != INVALID_HANDLE_VALUE) {
                SetConsoleMode(hIn, origInMode);
                SetConsoleCP(origInCP);
            }
            initialized = false;
        }
    }

    ~ConsoleGuard() {
        Restore();
    }
};

inline ConsoleGuard* ConsoleGuard::instance = nullptr;

// -----------------------------------------------------------------------------
// Win32 Process Execution Engine (Deadlock-Free Async Pipes & Safe Spawning)
// -----------------------------------------------------------------------------

class Sys {
public:
    class ScopedHandle {
    public:
        ScopedHandle() = default;
        explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;
        ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.release()) {}
        ScopedHandle& operator=(ScopedHandle&& other) noexcept {
            if (this != &other) reset(other.release());
            return *this;
        }
        ~ScopedHandle() { reset(); }

        HANDLE get() const { return handle_; }
        HANDLE release() {
            HANDLE handle = handle_;
            handle_ = INVALID_HANDLE_VALUE;
            return handle;
        }
        void reset(HANDLE handle = INVALID_HANDLE_VALUE) {
            if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL) CloseHandle(handle_);
            handle_ = handle;
        }
        bool valid() const { return handle_ != INVALID_HANDLE_VALUE && handle_ != NULL; }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };

    static constexpr size_t kMaxCapturedOutputBytes = 8 * 1024 * 1024;

    static std::wstring Utf8ToWide(const std::string& text) {
        if (text.empty()) return {};
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), NULL, 0);
        if (length <= 0) return std::wstring(text.begin(), text.end());
        std::wstring result(length, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    static std::wstring QuoteArg(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
        std::wstring out = L"\"";
        for (auto it = arg.begin(); it != arg.end(); ++it) {
            unsigned int backslashes = 0;
            while (it != arg.end() && *it == L'\\') {
                ++it;
                ++backslashes;
            }
            if (it == arg.end()) {
                out.append(backslashes * 2, L'\\');
                break;
            } else if (*it == L'"') {
                out.append(backslashes * 2 + 1, L'\\');
                out.push_back(*it);
            } else {
                out.append(backslashes, L'\\');
                out.push_back(*it);
            }
        }
        out += L"\"";
        return out;
    }

    static bool IsValidArg(const std::wstring& value) {
        if (value.empty()) return false;
        for (wchar_t ch : value) {
            if (ch == L'\0') return false;
            if (ch < 0x20 || ch == 0x7F) return false;
            if (ch == L'"' || ch == L'|' || ch == L'&' || ch == L';' || ch == L'`') return false;
        }
        return true;
    }

    static bool IsValidDistroName(const std::wstring& value) {
        if (!IsValidArg(value)) return false;
        if (value == L"." || value == L"..") return false;
        if (value.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return false;
        if (value.front() == L' ' || value.back() == L' ') return false;
        return true;
    }

    static bool IsValidFilesystemPath(const std::wstring& value, bool requireExistingFile = false) {
        if (!IsValidArg(value)) return false;
        if (value.find_first_of(L"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0B\x0C\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F") != std::wstring::npos) return false;
        if (value.length() > 32767) return false;
        if (value.find(L"\\\\?\\") == 0 || value.find(L"//") == 0 || value.find(L"\\\\") == 0) {
            return false;
        }
        if (requireExistingFile) {
            const fs::path p = fs::path(value);
            return fs::exists(p) && fs::is_regular_file(p);
        }
        return true;
    }

    static std::wstring ResolveExecutable(const std::wstring& name) {
        wchar_t systemDir[MAX_PATH] = {};
        const UINT size = GetSystemDirectoryW(systemDir, MAX_PATH);
        if (size > 0 && size < MAX_PATH) {
            const std::wstring candidate = std::wstring(systemDir) + L"\\" + name;
            const DWORD attributes = GetFileAttributesW(candidate.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                return candidate;
            }
        }
        return L"";
    }

    static std::wstring ResolveWslcExecutable() {
        PWSTR programFiles = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFiles)) || !programFiles) {
            return L"";
        }
        std::wstring candidate = std::wstring(programFiles) + L"\\WSL\\wslc.exe";
        CoTaskMemFree(programFiles);
        const DWORD attributes = GetFileAttributesW(candidate.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
            ? candidate
            : L"";
    }

    static std::wstring ResolveTerminalExecutable() {
        DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required <= 1) return L"";
        std::vector<wchar_t> localAppData(required);
        DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData.data(), required);
        if (length == 0 || length >= required) return L"";

        std::wstring candidate(localAppData.data(), length);
        candidate += L"\\Microsoft\\WindowsApps\\wt.exe";
        const DWORD attributes = GetFileAttributesW(candidate.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
            ? candidate
            : L"";
    }

    static std::string DecodeBuffer(const std::vector<char>& raw) {
        if (raw.empty()) return "";

        // Check for UTF-16LE or null-separated byte outputs
        if (raw.size() >= 2 && (raw[1] == '\0' || ((unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE))) {
            size_t offset = ((unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) ? 2 : 0;
            std::vector<wchar_t> wide;
            wide.reserve((raw.size() - offset) / 2);
            for (size_t i = offset; i + 1 < raw.size(); i += 2) {
                wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(raw[i]) |
                    (static_cast<unsigned char>(raw[i + 1]) << 8)));
            }
            int wlen = static_cast<int>(wide.size());
            int u8len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, NULL, 0, NULL, NULL);
            if (u8len > 0) {
                std::string out(u8len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, &out[0], u8len, NULL, NULL);
                out.erase(std::remove(out.begin(), out.end(), '\0'), out.end());
                out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
                return out;
            }
        }

        std::string out(raw.begin(), raw.end());
        out.erase(std::remove(out.begin(), out.end(), '\0'), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
        return out;
    }

    static ProcessResult Exec(const std::vector<std::wstring>& args, DWORD timeoutMs = 30000) {
        return ExecWithExecutable(ResolveExecutable(L"wsl.exe"), args, timeoutMs);
    }

    static ProcessResult ExecWithExecutable(const std::wstring& executable, const std::vector<std::wstring>& args, DWORD timeoutMs = 30000) {
        ProcessResult res;
        std::wstring wslExe = executable;
        if (!IsValidArg(wslExe)) {
            res.stdErr = "Could not resolve the system wsl.exe path.";
            return res;
        }

        std::wstring cmdLine = QuoteArg(wslExe);
        for (const auto& a : args) {
            if (!IsValidArg(a)) {
                res.stdErr = "Rejected invalid WSL argument.";
                return res;
            }
            cmdLine += L" " + QuoteArg(a);
        }

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE rawOutRead = INVALID_HANDLE_VALUE, rawOutWrite = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&rawOutRead, &rawOutWrite, &sa, 0)) {
            res.stdErr = "Failed to create stdout pipe.";
            return res;
        }
        ScopedHandle hOutRead(rawOutRead), hOutWrite(rawOutWrite);
        HANDLE rawErrRead = INVALID_HANDLE_VALUE, rawErrWrite = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&rawErrRead, &rawErrWrite, &sa, 0)) {
            res.stdErr = "Failed to create stderr pipe.";
            return res;
        }
        ScopedHandle hErrRead(rawErrRead), hErrWrite(rawErrWrite);
        if (!SetHandleInformation(hOutRead.get(), HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(hErrRead.get(), HANDLE_FLAG_INHERIT, 0)) {
            res.stdErr = "Failed to configure child pipe handles.";
            return res;
        }

        ScopedHandle hNullInput(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hNullInput.valid()) {
            res.stdErr = "Failed to open NUL for child stdin.";
            return res;
        }

        ScopedHandle hJob(CreateJobObjectW(NULL, NULL));
        if (!hJob.valid()) {
            res.stdErr = "Failed to create subprocess job object.";
            return res;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(hJob.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            res.stdErr = "Failed to configure subprocess job object.";
            return res;
        }

        SIZE_T attributeListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
        if (attributeListSize == 0) {
            res.stdErr = "Failed to size process handle list.";
            return res;
        }
        std::vector<unsigned char> attributeStorage(attributeListSize);
        auto attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
        if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize)) {
            res.stdErr = "Failed to initialize process handle list.";
            return res;
        }
        struct AttributeListGuard {
            LPPROC_THREAD_ATTRIBUTE_LIST value;
            ~AttributeListGuard() { DeleteProcThreadAttributeList(value); }
        } attributeGuard{ attributeList };
        HANDLE inheritedHandles[] = { hOutWrite.get(), hErrWrite.get(), hNullInput.get() };
        if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inheritedHandles, sizeof(inheritedHandles), NULL, NULL)) {
            res.stdErr = "Failed to restrict inherited process handles.";
            return res;
        }

        STARTUPINFOEXW si = {};
        si.StartupInfo.cb = sizeof(si);
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.StartupInfo.hStdInput = hNullInput.get();
        si.StartupInfo.hStdOutput = hOutWrite.get();
        si.StartupInfo.hStdError = hErrWrite.get();
        si.StartupInfo.wShowWindow = SW_HIDE;
        si.lpAttributeList = attributeList;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');

        BOOL created = CreateProcessW(
            wslExe.c_str(), cmdBuf.data(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi
        );
        hOutWrite.reset();
        hErrWrite.reset();

        if (!created) {
            res.stdErr = "Failed to spawn wsl.exe process.";
            return res;
        }
        ScopedHandle hProcess(pi.hProcess), hThread(pi.hThread);

        if (!AssignProcessToJobObject(hJob.get(), hProcess.get())) {
            TerminateProcess(hProcess.get(), 1);
            WaitForSingleObject(hProcess.get(), INFINITE);
            res.stdErr = "Failed to assign wsl.exe to its subprocess job.";
            return res;
        }
        if (ResumeThread(hThread.get()) == static_cast<DWORD>(-1)) {
            TerminateJobObject(hJob.get(), 1);
            WaitForSingleObject(hProcess.get(), INFINITE);
            res.stdErr = "Failed to resume wsl.exe process.";
            return res;
        }

        struct StreamCapture {
            std::string text;
            bool truncated = false;
        };
        auto ReadStream = [](ScopedHandle handle) -> StreamCapture {
            StreamCapture capture;
            std::vector<char> buf;
            buf.reserve(64 * 1024);
            char temp[4096];
            DWORD read = 0;
            while (ReadFile(handle.get(), temp, sizeof(temp), &read, NULL) && read > 0) {
                const size_t room = kMaxCapturedOutputBytes - buf.size();
                const size_t toAppend = (std::min)(room, static_cast<size_t>(read));
                buf.insert(buf.end(), temp, temp + toAppend);
                if (toAppend < read) capture.truncated = true;
            }
            capture.text = DecodeBuffer(buf);
            if (capture.truncated) capture.text += "\n[...output truncated...]\n";
            return capture;
        };

        // Concurrent asynchronous pipe readers prevent pipe deadlocks
        std::future<StreamCapture> futureOut;
        std::future<StreamCapture> futureErr;
        try {
            futureOut = std::async(std::launch::async, ReadStream, std::move(hOutRead));
            futureErr = std::async(std::launch::async, ReadStream, std::move(hErrRead));
        } catch (...) {
            TerminateJobObject(hJob.get(), 1);
            WaitForSingleObject(hProcess.get(), 5000);
            hJob.reset();
            if (futureOut.valid()) futureOut.wait();
            res.stdErr = "Failed to start subprocess output readers.";
            return res;
        }

        DWORD waitRes = WaitForSingleObject(hProcess.get(), timeoutMs);
        if (waitRes == WAIT_TIMEOUT) {
            TerminateJobObject(hJob.get(), 1);
            WaitForSingleObject(hProcess.get(), 5000);
            res.exitCode = -1;
            res.stdErr = "[Timeout]: Subsystem operation exceeded timeout threshold.";
        } else if (waitRes == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(hProcess.get(), &code);
            res.exitCode = static_cast<int>(code);
        } else {
            TerminateJobObject(hJob.get(), 1);
            WaitForSingleObject(hProcess.get(), 5000);
            res.exitCode = -1;
            res.stdErr = "Failed while waiting for wsl.exe.";
        }

        hJob.reset();
        try {
            StreamCapture outCapture = futureOut.get();
            StreamCapture errCapture = futureErr.get();
            res.stdOut = std::move(outCapture.text);
            if (!errCapture.text.empty()) {
                if (res.stdErr.empty()) res.stdErr = std::move(errCapture.text);
                else res.stdErr += "\n" + errCapture.text;
            }
        } catch (...) {
            res.exitCode = -1;
            res.stdErr = "Failed to collect wsl.exe output.";
        }
        return res;
    }

    static bool LaunchInTerminal(const std::wstring& distroName) {
        if (!IsValidDistroName(distroName)) return false;

        const std::wstring wslExe = ResolveExecutable(L"wsl.exe");
        const std::wstring wtExe = ResolveTerminalExecutable();
        const std::wstring conhostExe = ResolveExecutable(L"conhost.exe");
        const std::wstring wtParams = QuoteArg(wslExe) + L" -d " + QuoteArg(distroName);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        if (!wtExe.empty()) {
            sei.lpFile = wtExe.c_str();
            sei.lpParameters = wtParams.c_str();
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) == TRUE) return true;
        }

        const std::wstring conhostParams = QuoteArg(wslExe) + L" -d " + QuoteArg(distroName);
        sei.lpFile = conhostExe.c_str();
        sei.lpParameters = conhostParams.c_str();
        return ShellExecuteExW(&sei) == TRUE;
    }

    static bool LaunchInstallWindow(const std::wstring& distroId) {
        if (!IsValidDistroName(distroId)) return false;

        const std::wstring wslExe = ResolveExecutable(L"wsl.exe");
        const std::wstring wtExe = ResolveTerminalExecutable();
        const std::wstring conhostExe = ResolveExecutable(L"conhost.exe");
        const std::wstring wtParams = QuoteArg(wslExe) + L" --install -d " + QuoteArg(distroId);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        if (!wtExe.empty()) {
            sei.lpFile = wtExe.c_str();
            sei.lpParameters = wtParams.c_str();
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) == TRUE) return true;
        }

        const std::wstring conhostParams = QuoteArg(wslExe) + L" --install -d " + QuoteArg(distroId);
        sei.lpFile = conhostExe.c_str();
        sei.lpParameters = conhostParams.c_str();
        return ShellExecuteExW(&sei) == TRUE;
    }

    static ProcessResult RunWithSpinner(const std::string& message, std::function<ProcessResult()> task) {
        std::atomic<bool> done{ false };
        auto futureRes = std::async(std::launch::async, [&]() {
            ProcessResult res;
            try {
                res = task();
            } catch (const std::exception& error) {
                res.exitCode = -1;
                res.stdErr = error.what();
            } catch (...) {
                res.exitCode = -1;
                res.stdErr = "Operation failed unexpectedly.";
            }
            done = true;
            return res;
        });

        const char spinnerChars[] = { '|', '/', '-', '\\' };
        int sIdx = 0;

        std::cout << "\033[?25l";
        while (!done) {
            std::cout << "\r " << Color::CYAN << "[" << spinnerChars[sIdx % 4] << "]" << Color::RESET 
                      << " " << message << "..." << Color::CLEAR_LINE << std::flush;
            sIdx++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "\r" << Color::CLEAR_LINE << std::flush;
        return futureRes.get();
    }
};

// -----------------------------------------------------------------------------
// Whitespace-Resilient WSL Data Parsing Engine
// -----------------------------------------------------------------------------

class WSL {
public:
    static std::vector<WslProcess> ListProcesses(const std::wstring& distroName) {
        std::vector<WslProcess> processes;
        ProcessResult result = Sys::Exec({ L"-d", distroName, L"--", L"ps", L"-eo", L"pid=,comm=,args=" }, 5000);
        if (!result.success()) return processes;

        std::istringstream lines(result.stdOut);
        std::string line;
        while (std::getline(lines, line)) {
            std::istringstream fields(line);
            WslProcess process;
            if (!(fields >> process.pid >> process.name)) continue;
            std::getline(fields, process.command);
            const size_t first = process.command.find_first_not_of(" \t");
            if (first != std::string::npos) process.command.erase(0, first);
            if (process.command.empty()) process.command = process.name;
            processes.push_back(process);
        }
        return processes;
    }

    static ProcessResult StopProcess(const std::wstring& distroName, unsigned long pid) {
        return Sys::Exec({ L"-d", distroName, L"--", L"kill", L"-TERM", std::to_wstring(pid) }, 5000);
    }

    static std::vector<DistroInfo> ListDistros() {
        std::vector<DistroInfo> list;
        ProcessResult r = Sys::Exec({ L"-l", L"-v" }, 5000);
        if (!r.success()) return list;

        std::istringstream iss(r.stdOut);
        std::string line;
        bool headerPassed = false;

        while (std::getline(iss, line)) {
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
            if (!headerPassed) {
                if (line.find("NAME") != std::string::npos && line.find("STATE") != std::string::npos) {
                    headerPassed = true;
                }
                continue;
            }

            std::istringstream lss(line);
            std::string tok;
            std::vector<std::string> toks;
            while (lss >> tok) toks.push_back(tok);

            if (toks.size() < 3) continue;

            DistroInfo d;
            size_t startIdx = 0;
            if (toks[0] == "*") {
                d.isDefault = true;
                startIdx = 1;
            }

            // Right-to-Left parsing guarantees distro names with spaces are preserved
            d.version = toks.back();
            toks.pop_back();
            d.state = toks.back();
            toks.pop_back();

            std::string nameBuilder;
            for (size_t i = startIdx; i < toks.size(); ++i) {
                if (!nameBuilder.empty()) nameBuilder += " ";
                nameBuilder += toks[i];
            }
            d.name = nameBuilder;

            if (!d.name.empty()) list.push_back(d);
        }
        return list;
    }

    static std::vector<OnlineDistro> ListOnline() {
        std::vector<OnlineDistro> catalog;
        ProcessResult r = Sys::Exec({ L"--list", L"--online" }, 10000);
        if (!r.success()) return catalog;

        std::istringstream iss(r.stdOut);
        std::string line;
        bool headerPassed = false;

        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            if (line.find("NAME") != std::string::npos && line.find("FRIENDLY NAME") != std::string::npos) {
                headerPassed = true;
                continue;
            }
            if (headerPassed) {
                std::istringstream lss(line);
                std::string id, friendly;
                if (lss >> id) {
                    if (id == "*") {
                        if (!(lss >> id)) continue;
                    }
                    std::getline(lss, friendly);
                    friendly.erase(0, friendly.find_first_not_of(" \t"));
                    catalog.push_back({ id, friendly.empty() ? id : friendly });
                }
            }
        }
        return catalog;
    }

    static ProcessResult ListContainers(std::vector<WslContainer>& containers) {
        containers.clear();
        const std::wstring wslcExe = Sys::ResolveWslcExecutable();
        if (wslcExe.empty()) {
            return { -1, "", "Could not find wslc.exe under the Program Files WSL installation." };
        }

        ProcessResult result = Sys::ExecWithExecutable(wslcExe, { L"list", L"--all", L"--quiet" }, 15000);
        if (!result.success()) return result;

        std::istringstream lines(result.stdOut);
        std::string line;
        while (std::getline(lines, line)) {
            const size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            const size_t last = line.find_last_not_of(" \t\r\n");
            line = line.substr(first, last - first + 1);
            if (line.size() > 256 || !Sys::IsValidArg(Sys::Utf8ToWide(line))) {
                containers.clear();
                return { -1, "", "wslc returned an invalid container ID." };
            }
            containers.push_back({ line });
        }
        return result;
    }

    static ProcessResult RunContainerAction(const std::wstring& action, const std::string& id) {
        if (action != L"start" && action != L"stop" && action != L"remove") {
            return { -1, "", "Unsupported wslc container action." };
        }
        const std::wstring wideId = Sys::Utf8ToWide(id);
        if (id.empty() || id.size() > 256 || !Sys::IsValidArg(wideId)) {
            return { -1, "", "Invalid container ID." };
        }
        const std::wstring wslcExe = Sys::ResolveWslcExecutable();
        if (wslcExe.empty()) {
            return { -1, "", "Could not find wslc.exe under the Program Files WSL installation." };
        }
        return Sys::ExecWithExecutable(wslcExe, { action, wideId }, 120000);
    }
};

// -----------------------------------------------------------------------------
// Interactive TUI Application Controller
// -----------------------------------------------------------------------------

