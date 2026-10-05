    enum class Level { Debug, Verbose, Status, Warning, Error };
    static Level CurrentLevel = Level::Status;

    void InitEnvironment() noexcept {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        // 1. Lock down search paths permanently against binary hijacking
        ::SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

        // 2. Enforce UTF-8 Code Page on Windows Console
        ::SetConsoleOutputCP(CP_UTF8);
        ::SetConsoleCP(CP_UTF8);

        // 3. Enable VT Processing on standard output handle
        HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            DWORD dwMode = 0;
            if (::GetConsoleMode(hOut, &dwMode)) {
                ::SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    }

    void Message(Level lvl, std::string_view msg) {
        if (lvl < CurrentLevel) return;
        switch (lvl) {
            case Level::Debug:
                std::cout << "\x1b[36m-- [DEBUG] " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Verbose:
                std::cout << "\x1b[37m-- " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Status:
                std::cout << "\x1b[32m--\x1b[0m " << msg << "\n" << std::flush;
                break;
            case Level::Warning:
                std::cerr << "\x1b[33mCMake Warning: " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Error:
                std::cerr << "\x1b[31mCMake Error: " << msg << "\x1b[0m\n" << std::flush;
                break;
        }
    }
}

// ============================================================================
// Signal & Break Handling
// ============================================================================
static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            Log::Message(Log::Level::Warning, "Termination signal received. Killing active process tree...");
            GlobalJobTracker::Instance().TerminateAll(STATUS_CONTROL_C_EXIT);
            return TRUE;
        default:
            return FALSE;
    }
}

// ============================================================================
// Data Model & CLI Configuration Options
// ============================================================================
