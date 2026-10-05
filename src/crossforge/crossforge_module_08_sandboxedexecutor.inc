class SandboxedExecutor {
public:
    static DWORD GetProcessTimeoutMs() {
        const DWORD defaultMs = 180000;
        const DWORD minMs = 1000;
        const DWORD maxMs = 600000;

        char envBuf[64] = {0};
        DWORD len = GetEnvironmentVariableA("crossforge_PROCESS_TIMEOUT_MS", envBuf, static_cast<DWORD>(sizeof(envBuf)));
        if (len == 0 || len >= sizeof(envBuf)) {
            return defaultMs;
        }

        try {
            unsigned long parsed = std::stoul(std::string(envBuf, len));
            return static_cast<DWORD>((std::max)(static_cast<unsigned long>(minMs), (std::min)(parsed, static_cast<unsigned long>(maxMs))));
        } catch (const std::exception&) {
            return defaultMs;
        } catch (...) {
            return defaultMs;
        }
    }

    static DWORD GetActiveProcessLimit() {
        const DWORD defaultLimit = 64;
        const DWORD minLimit = 1;
        const DWORD maxLimit = 512;

        char envBuf[64] = {0};
        DWORD len = GetEnvironmentVariableA("crossforge_PROCESS_MAX_CHILDREN", envBuf, static_cast<DWORD>(sizeof(envBuf)));
        if (len == 0 || len >= sizeof(envBuf)) {
            return defaultLimit;
        }

        try {
            unsigned long parsed = std::stoul(std::string(envBuf, len));
            return static_cast<DWORD>((std::max)(static_cast<unsigned long>(minLimit), (std::min)(parsed, static_cast<unsigned long>(maxLimit))));
        } catch (const std::exception&) {
            return defaultLimit;
        } catch (...) {
            return defaultLimit;
        }
    }

    static std::string ResolveShellPath() {
        char sysDir[MAX_PATH] = {0};
        UINT sysLen = GetSystemDirectoryA(sysDir, MAX_PATH);
        if (sysLen > 0 && sysLen < MAX_PATH) {
            return std::string(sysDir, sysLen) + "\\cmd.exe";
        }
        return "C:\\Windows\\System32\\cmd.exe";
    }

    static std::string StripCmdPrefix(const std::string& command) {
        auto trim = [](std::string s) {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
            return s.substr(start, end - start);
        };

        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        };

        std::string trimmed = trim(command);
        if (trimmed.empty()) {
            return trimmed;
        }

        size_t tokenEnd = 0;
        if (!trimmed.empty() && trimmed[0] == '"') {
            size_t quoteEnd = trimmed.find('"', 1);
            tokenEnd = (quoteEnd == std::string::npos) ? trimmed.size() : quoteEnd + 1;
        } else {
            while (tokenEnd < trimmed.size() && !std::isspace(static_cast<unsigned char>(trimmed[tokenEnd]))) {
                ++tokenEnd;
            }
        }

        std::string token = trimmed.substr(0, tokenEnd);
        std::string tokenLower = toLower(token);

        bool isCmd = (tokenLower == "cmd" || tokenLower == "cmd.exe" ||
                      tokenLower.find("\\cmd.exe") != std::string::npos ||
                      tokenLower.find("/cmd.exe") != std::string::npos);
        if (!isCmd) {
            return trimmed;
        }

        std::string rest = trim(trimmed.substr(tokenEnd));
        // Strip cmd switches that precede the command string: /c, and any /v:on
        // (delayed-expansion) flag that may appear before /c.
        while (rest.size() >= 2 && rest[0] == '/') {
            char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(rest[1])));
            if (c1 == 'c') { rest = trim(rest.substr(2)); break; }
            // Skip a non-/c switch token (e.g. /v:on, /d) and keep scanning.
            size_t sp = 1;
            while (sp < rest.size() && !std::isspace(static_cast<unsigned char>(rest[sp]))) ++sp;
            rest = trim(rest.substr(sp));
        }
        return rest;
    }

    // Non-destructively check whether the user pressed ESC on stdin, and consume
    // it if so. Handles both real console input handles (conhost/Windows
    // Terminal) and pipe/PTY stdin.
    static bool CheckEscapeCancel(HANDLE inputHandle) {
        if (inputHandle == INVALID_HANDLE_VALUE || inputHandle == nullptr) return false;

        if (GetFileType(inputHandle) == FILE_TYPE_CHAR) {
            // Console input buffer: PeekNamedPipe fails here, so use console APIs.
            DWORD numEvents = 0;
            if (!GetNumberOfConsoleInputEvents(inputHandle, &numEvents) || numEvents == 0) {
                return false;
            }
            std::vector<INPUT_RECORD> buf(numEvents);
            DWORD peeked = 0;
            if (!PeekConsoleInputW(inputHandle, buf.data(), numEvents, &peeked)) return false;
            for (DWORD i = 0; i < peeked; ++i) {
                const INPUT_RECORD& rec = buf[i];
                if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown &&
                    rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) {
                    // Consume events up to and including the ESC so it doesn't leak
                    // into the editor's input queue after the process exits.
                    std::vector<INPUT_RECORD> discard(i + 1);
                    DWORD consumed = 0;
                    ReadConsoleInputW(inputHandle, discard.data(), i + 1, &consumed);
                    return true;
                }
            }
            return false;
        }

        // Pipe / PTY stdin: ESC arrives as a raw 0x1B byte.
        DWORD avail = 0;
        if (!PeekNamedPipe(inputHandle, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) {
            return false;
        }
        unsigned char firstByte = 0;
        DWORD peekRead = 0;
        if (PeekNamedPipe(inputHandle, &firstByte, 1, &peekRead, nullptr, nullptr) &&
            peekRead > 0 && firstByte == 0x1B) {
            char discard;
            DWORD consumed = 0;
            ReadFile(inputHandle, &discard, 1, &consumed, nullptr);
            return true;
        }
        return false;
    }

    static bool RunProcess(const std::string& command, std::string& outputText) {
        // Create a restricted Windows Job Object for Sandboxing child processes
        HANDLE hJob = CreateJobObjectW(NULL, NULL);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            jeli.BasicLimitInformation.ActiveProcessLimit = GetActiveProcessLimit();
            if (jeli.BasicLimitInformation.ActiveProcessLimit > 0) {
                jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            }
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }

        SECURITY_ATTRIBUTES saAttr{};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
            if (hJob) CloseHandle(hJob);
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.hStdOutput = hWritePipe;
        si.hStdError  = hWritePipe;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi{};
        const std::string shellPath = ResolveShellPath();
        const std::string shellCommand = "\"" + shellPath + "\" /d /c " + StripCmdPrefix(command);
        std::wstring shellPathW = Utf8ToWide(shellPath);
        std::wstring shellCommandW = Utf8ToWide(shellCommand);
        // CreateProcessW requires a writable command-line buffer.
        std::vector<wchar_t> cmdBuf(shellCommandW.begin(), shellCommandW.end());
        cmdBuf.push_back(L'\0');

        BOOL success = CreateProcessW(
            shellPathW.empty() ? nullptr : shellPathW.c_str(), cmdBuf.data(), NULL, NULL, TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi
        );

        if (!success) {
            outputText += "\n[CrossForge] Failed to launch process (CreateProcessW error " + std::to_string(GetLastError()) + ").";
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            if (hJob) CloseHandle(hJob);
            return false;
        }

        if (hJob && !AssignProcessToJobObject(hJob, pi.hProcess)) {
            outputText += "\n[CrossForge] Failed to assign process to sandbox job object.";
            TerminateProcess(pi.hProcess, 125);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            CloseHandle(hJob);
            return false;
        }

        if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
            outputText += "\n[CrossForge] Failed to start process thread.";
            TerminateProcess(pi.hProcess, 126);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            if (hJob) CloseHandle(hJob);
            return false;
        }

        CloseHandle(hWritePipe);

        const DWORD timeoutMs = GetProcessTimeoutMs();
        ULONGLONG deadline = GetTickCount64() + timeoutMs;
        bool timedOut = false;
        bool cancelled = false;
        bool readError = false;

        // Event-driven wait: a reader thread performs blocking ReadFile on the
        // child's pipe and signals hPipeDataEvent whenever output arrives (or the
        // pipe closes). The main thread waits on { process exit, pipe data } with
        // a bounded slice so ESC-cancel and the deadline stay responsive without
        // a Sleep() spin loop.
        HANDLE hPipeDataEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
        struct PipeReaderState {
            HANDLE pipe = nullptr;
            HANDLE dataEvent = nullptr;
            std::string* output = nullptr;
            std::atomic<bool> stop{ false };
            std::atomic<bool> readError{ false };
            std::mutex mtx;
        };
        PipeReaderState readerState{ hReadPipe, hPipeDataEvent, &outputText, {}, {}, {} };

        std::thread pipeReader([&readerState]() {
            char buffer[1024];
            while (!readerState.stop.load()) {
                DWORD bytesRead = 0;
                BOOL okRead = ReadFile(readerState.pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
                if (!okRead || bytesRead == 0) {
                    // Pipe closed (child exited / error). Signal so the waiter wakes.
                    if (readerState.dataEvent) SetEvent(readerState.dataEvent);
                    if (!okRead) readerState.readError.store(true);
                    break;
                }
                buffer[bytesRead] = '\0';
                {
                    std::lock_guard<std::mutex> lock(readerState.mtx);
                    readerState.output->append(buffer, static_cast<size_t>(bytesRead));
                }
                if (readerState.dataEvent) SetEvent(readerState.dataEvent);
            }
        });

        auto flushPipeOutput = [&]() {
            std::lock_guard<std::mutex> lock(readerState.mtx);
            // Output already appended by the reader thread; nothing to do here.
            // (kept for symmetry / future per-chunk processing)
        };

        HANDLE waitHandles[2] = { pi.hProcess, hPipeDataEvent };
        while (true) {
            // Compute remaining time so we wake promptly at the deadline even if
            // no events fire; 50 ms slices keep ESC-cancel responsive.
            ULONGLONG now = GetTickCount64();
            DWORD slice = 50;
            if (now < deadline) {
                ULONGLONG remaining = deadline - now;
                if (remaining < slice) slice = static_cast<DWORD>(remaining);
            } else {
                slice = 0;
            }

            DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, slice);
            flushPipeOutput();

            if (waitResult == WAIT_OBJECT_0) {
                // Process exited. Let the reader finish draining, then stop.
                break;
            }
            if (waitResult == WAIT_FAILED) {
                outputText += "\n[CrossForge] Failed waiting on process handle.";
                readError = true;
                break;
            }
            // WAIT_OBJECT_0+1 (pipe data) or WAIT_TIMEOUT: check cancel/deadline.

            // ESC-cancel. stdin can be a real console input handle (conhost /
            // Windows Terminal, FILE_TYPE_CHAR) or a pipe/PTY. PeekNamedPipe only
            // works on pipes, so pick the right peek per handle type.
            HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
            if (inputHandle != INVALID_HANDLE_VALUE && CheckEscapeCancel(inputHandle)) {
                cancelled = true;
                outputText += "\n[CrossForge] Process cancelled by user.";
                TerminateProcess(pi.hProcess, 123);
                WaitForSingleObject(pi.hProcess, 2000);
                break;
            }

            if (GetTickCount64() >= deadline) {
                timedOut = true;
                outputText += "\n[CrossForge] Process timed out after " + std::to_string(timeoutMs) + " ms.";
                TerminateProcess(pi.hProcess, 124);
                WaitForSingleObject(pi.hProcess, 2000);
                break;
            }
        }

        // Stop and join the reader thread, then close the event.
        readerState.stop.store(true);
        // Nudge the reader out of a blocking ReadFile if the child is gone but
        // the pipe is somehow still open: cancel pending I/O on this pipe.
        CancelIoEx(hReadPipe, nullptr);
        if (pipeReader.joinable()) pipeReader.join();
        if (readerState.readError.load()) readError = true;
        if (hPipeDataEvent) CloseHandle(hPipeDataEvent);

        bool ok = false;
        if (!timedOut && !cancelled && !readError) {
            DWORD exitCode = 1;
            if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
                ok = (exitCode == 0);
                if (!ok) {
                    outputText += "\n[CrossForge] Process exited with code " + std::to_string(exitCode) + ".";
                }
            } else {
                outputText += "\n[CrossForge] Failed to query process exit code.";
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        if (hJob) CloseHandle(hJob);

        return ok;
    }
};

// ============================================================================
// DOCUMENT & EDITOR SYSTEM
// ============================================================================

// Cross-line lexical state for the syntax highlighter. Carried from line to
// line so block comments and multi-line strings stay highlighted.
