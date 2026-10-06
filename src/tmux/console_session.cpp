#include "shell_command.hpp"
#include "wmux_engine.hpp"

bool WmuxEngine::InitConsole() {
        GetConsoleMode(hStdIn, &origInMode);
        GetConsoleMode(hStdOut, &origOutMode);

        DWORD outMode = origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        DWORD inMode = origInMode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
        inMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

        SetConsoleMode(hStdOut, outMode);
        SetConsoleMode(hStdIn, inMode);

        std::cout << VT_ALT_BUF_ON << VT_HIDE_CURSOR << VT_MOUSE_ON << std::flush;
        UpdateSize();
        CreateWindowTab(ShellTabName(defaultShell), defaultShell);
        return true;
    }

void WmuxEngine::RestoreConsole() {
        std::cout << VT_MOUSE_OFF << VT_SHOW_CURSOR << VT_ALT_BUF_OFF << std::flush;
        SetConsoleMode(hStdIn, origInMode);
        SetConsoleMode(hStdOut, origOutMode);
    }

void WmuxEngine::UpdateSize() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
            int newW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int newH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            if (newW != termW || newH != termH) {
                termW = newW;
                termH = newH;
                // Clear the console to avoid layout remnants
                DWORD written;
                WriteConsoleW(hStdOut, L"\x1b[2J", 5, &written, NULL);
            }
        }
    }
