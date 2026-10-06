#include "engine.hpp"

// ConsoleModeGuard
ConsoleModeGuard::ConsoleModeGuard(HANDLE h) : hIn(h) {
    if (GetConsoleMode(hIn, &origMode)) {
        DWORD newMode = origMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        if (SetConsoleMode(hIn, newMode)) {
            active = true;
        }
    }
}

ConsoleModeGuard::~ConsoleModeGuard() {
    restore();
}

void ConsoleModeGuard::restore() {
    if (active) {
        SetConsoleMode(hIn, origMode);
        active = false;
    }
}

// ConsoleReader
int ConsoleReader::read(const ReadOptions& opts, std::wstring& result) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    ConsoleModeGuard guard(hIn);

    auto startTime = std::chrono::steady_clock::now();
    bool escapeNext = false;

    while (true) {
        if (opts.maxChars > 0 && static_cast<int>(result.length()) >= opts.maxChars) {
            break;
        }

        DWORD waitMs = INFINITE;
        if (opts.timeoutSec >= 0.0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double remaining = opts.timeoutSec - elapsed;
            if (remaining <= 0.0) {
                return 142; // Timeout status code
            }
            waitMs = static_cast<DWORD>(remaining * 1000.0);
        }

        DWORD waitRes = WaitForSingleObject(hIn, waitMs);
        if (waitRes == WAIT_TIMEOUT) {
            return 142;
        }
        if (waitRes != WAIT_OBJECT_0) {
            return 1;
        }

        INPUT_RECORD ir;
        DWORD recordsRead = 0;
        if (!ReadConsoleInputW(hIn, &ir, 1, &recordsRead) || recordsRead == 0) {
            return 1;
        }

        if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
            continue;
        }

        wchar_t ch = ir.Event.KeyEvent.uChar.UnicodeChar;
        WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;

        if (ch == 0) continue;

        if (ch == 3) return 130; // Ctrl+C
        if (ch == 26) break;     // Ctrl+Z (EOF)

        if (vk == VK_BACK || ch == L'\b') {
            if (!result.empty()) {
                result.pop_back();
                if (!opts.silent) {
                    DWORD written = 0;
                    WriteConsoleW(hOut, L"\b \b", 3, &written, NULL);
                }
            }
            continue;
        }

        if (escapeNext && (ch == L'\r' || ch == L'\n')) {
            escapeNext = false;
            if (!opts.silent) {
                DWORD written = 0;
                WriteConsoleW(hOut, L"\r\n> ", 4, &written, NULL);
            }
            continue;
        }

        bool isDelim = false;
        if (opts.delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
            isDelim = true;
        } else if (ch == opts.delim) {
            isDelim = true;
        }

        if (isDelim) {
            if (!opts.silent) {
                DWORD written = 0;
                WriteConsoleW(hOut, L"\r\n", 2, &written, NULL);
            }
            break;
        }

        if (!opts.raw && !escapeNext && ch == L'\\') {
            escapeNext = true;
            continue;
        }

        escapeNext = false;
        result.push_back(ch);

        if (!opts.silent) {
            DWORD written = 0;
            WriteConsoleW(hOut, &ch, 1, &written, NULL);
        }
    }

    return 0;
}

// PipeReader
int PipeReader::read(const ReadOptions& opts, std::wstring& result) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    auto startTime = std::chrono::steady_clock::now();
    bool escapeNext = false;

    while (true) {
        if (opts.maxChars > 0 && static_cast<int>(result.length()) >= opts.maxChars) {
            break;
        }

        if (opts.timeoutSec >= 0.0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            if (elapsed >= opts.timeoutSec) {
                return 142;
            }

            DWORD avail = 0;
            if (PeekNamedPipe(hIn, NULL, 0, NULL, &avail, NULL) && avail == 0) {
                Sleep(10);
                continue;
            }
        }

        char c = 0;
        DWORD bytesRead = 0;
        if (!ReadFile(hIn, &c, 1, &bytesRead, NULL) || bytesRead == 0) {
            break;
        }

        wchar_t ch = static_cast<wchar_t>(c);

        if (opts.delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
            if (ch == L'\r') {
                char nextC = 0;
                DWORD peekRead = 0;
                if (PeekNamedPipe(hIn, &nextC, 1, &peekRead, NULL, NULL) && peekRead > 0 && nextC == '\n') {
                    ReadFile(hIn, &nextC, 1, &peekRead, NULL);
                }
            }
            break;
        } else if (ch == opts.delim) {
            break;
        }

        if (!opts.raw && !escapeNext && ch == L'\\') {
            escapeNext = true;
            continue;
        }

        escapeNext = false;
        result.push_back(ch);
    }

    return 0;
}

// ReadEngine
ReadEngine::ReadEngine(ReadOptions opts) : options(std::move(opts)) {}

int ReadEngine::execute() {
    if (!options.prompt.empty()) {
        ReadReporter::writeHandle(GetStdHandle(STD_ERROR_HANDLE), options.prompt);
    }

    std::wstring result;
    int exitCode = 0;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
        exitCode = ConsoleReader::read(options, result);
    } else {
        exitCode = PipeReader::read(options, result);
    }

    return ReadReporter::output(result, options, exitCode);
}
