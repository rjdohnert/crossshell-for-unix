// ============================================================================
// 2. TERMINAL DISPLAY CONTROLLER
// ============================================================================

class TerminalDisplay {
public:
    static void enableVtMode() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }

    static void getSize(int& width, int& height) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        } else {
            width = 80;
            height = 25;
        }
    }

    static KeyCode readKey() {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0 || ch == 224) {
            int ext = _getch();
            switch (ext) {
                case 72: return KeyCode::UP;
                case 80: return KeyCode::DOWN;
                case 73: return KeyCode::PAGE_UP;
                case 81: return KeyCode::PAGE_DOWN;
                case 71: return KeyCode::HOME;
                case 79: return KeyCode::END;
                default: return KeyCode::OTHER;
            }
        }

        switch (ch) {
            case 'q': case 'Q': return KeyCode::QUIT;
            case 'h': case 'H': return KeyCode::HELP;
            case 'j': case '\r': case '\n': return KeyCode::DOWN;
            case 'k': case 'y': return KeyCode::UP;
            case 'f': case ' ': return KeyCode::PAGE_DOWN;
            case 'b': return KeyCode::PAGE_UP;
            case 'g': return KeyCode::HOME;
            case 'G': return KeyCode::END;
            case '/': return KeyCode::SEARCH_FWD;
            case '?': return KeyCode::SEARCH_BWD;
            case 'n': return KeyCode::SEARCH_NEXT;
            case 'N': return KeyCode::SEARCH_PREV;
            case ':': return KeyCode::NEXT_FILE;
            default: return KeyCode::OTHER;
        }
    }
};

