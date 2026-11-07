#include "process_monitor.hpp"

    HpUxTopEngine::HpUxTopEngine(const TopOptions& options)
        : refreshDelaySec(options.refreshDelaySec),
          maxDisplayCount(options.maxDisplayCount),
          currentSort(options.currentSort),
          userFilter(options.userFilter) {
        InitSystemSnapshot();
    }

    HpUxTopEngine::~HpUxTopEngine() {
        if (cursorHidden) {
            std::cout << "\033[?25h\033[0m\r\n";
        }
        if (consoleModeSaved) {
            SetConsoleMode(consoleOutput, originalConsoleMode);
        }
    }

    bool HpUxTopEngine::EnableVirtualTerminal() {
        consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (consoleOutput == nullptr || consoleOutput == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(consoleOutput, &originalConsoleMode)) {
            std::cerr << "top: an interactive console is required for output\r\n";
            return false;
        }

        DWORD mode = originalConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (mode != originalConsoleMode && !SetConsoleMode(consoleOutput, mode)) {
            std::cerr << "top: unable to enable virtual-terminal processing (error "
                      << GetLastError() << ")\r\n";
            return false;
        }

        consoleModeSaved = true;
        std::cout << "\033[?25l";
        cursorHidden = true;
        return true;
    }

    int HpUxTopEngine::Run() {
        HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);
        DWORD inputMode = 0;
        if (consoleInput == nullptr || consoleInput == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(consoleInput, &inputMode)) {
            std::cerr << "top: an interactive console is required for input\r\n";
            return 2;
        }
        if (!EnableVirtualTerminal()) {
            return 2;
        }

        std::cout << "\033[2J";
        while (running) {
            Update();
            const uint64_t slices = static_cast<uint64_t>(refreshDelaySec) * 10;
            for (uint64_t i = 0; i < slices && running; ++i) {
                if (_kbhit()) {
                    HandleInput();
                    break;
                }
                Sleep(100);
            }
        }
        return 0;
    }
