#include "process_monitor.hpp"

HpUxTopEngine::HpUxTopEngine() {
        EnableVirtualTerminal();
        InitSystemSnapshot();
    }

HpUxTopEngine::~HpUxTopEngine() {
        // Show cursor again on exit
        std::cout << "\033[?25h\033[0m\n";
    }

void HpUxTopEngine::EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
        std::cout << "\033[?25l"; // Hide cursor
    }

void HpUxTopEngine::Run() {
        std::cout << "\033[2J"; // Clear screen initially
        while (true) {
            Update();
            // Non-blocking sleep loop checking keyboard input every 100ms
            int slices = refreshDelaySec * 10;
            for (int i = 0; i < slices; ++i) {
                if (_kbhit()) {
                    HandleInput();
                    break;
                }
                Sleep(100);
            }
        }
    }
