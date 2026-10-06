#include "tty_engine.hpp"
#include "tty_options.hpp"

bool TtyEngine::isTerminal(HANDLE hInput, std::wstring& ttyName) {
        if (hInput == INVALID_HANDLE_VALUE || hInput == NULL) {
            return false;
        }

        // 1. Check for Native Windows Console (cmd.exe, PowerShell, Windows Terminal)
        DWORD consoleMode = 0;
        if (GetConsoleMode(hInput, &consoleMode)) {
            ttyName = L"CONIN$";
            return true;
        }

        // 2. Check for MSYS2 / Cygwin / Git Bash PTY Pseudo-terminals
        DWORD fileType = GetFileType(hInput);
        if (fileType == FILE_TYPE_PIPE) {
            wchar_t pipeName[MAX_PATH] = { 0 };
            if (GetFinalPathNameByHandleW(hInput, pipeName, MAX_PATH, VOLUME_NAME_NT) > 0) {
                std::wstring name(pipeName);
                if (name.find(L"msys-") != std::wstring::npos ||
                    name.find(L"cygwin-") != std::wstring::npos ||
                    name.find(L"pty") != std::wstring::npos) {
                    ttyName = name;
                    return true;
                }
            }
        }

        return false;
    }

int TtyEngine::execute(const TtyOptions& opts) {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        std::wstring ttyName;

        if (isTerminal(hStdin, ttyName)) {
            if (!opts.silent) {
                std::wcout << ttyName << L"\n";
            }
            return 0; // 0 = Connected to TTY
        } else {
            if (!opts.silent) {
                std::wcout << L"not a tty\n";
            }
            return 1; // 1 = Not a TTY
        }
    }
