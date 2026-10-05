// ============================================================================
// CONSOLE & TERMINAL RENDER ENGINE (Double-Buffered VT100)
// ============================================================================
class ConsoleRenderer {
private:
    HANDLE m_hOut;
    HANDLE m_hIn;
    DWORD m_origOutMode;
    DWORD m_origInMode;
    SHORT m_width = 80;
    SHORT m_height = 25;
    bool m_initialized = false;
    bool m_isConsoleInput = false;

    std::wstring m_buffer;

public:
    ConsoleRenderer() : m_hOut(INVALID_HANDLE_VALUE), m_hIn(INVALID_HANDLE_VALUE) {}

    bool Initialize() {
        m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        m_hIn = GetStdHandle(STD_INPUT_HANDLE);

        if (m_hOut == INVALID_HANDLE_VALUE || m_hIn == INVALID_HANDLE_VALUE) return false;

        if (!GetConsoleMode(m_hOut, &m_origOutMode)) return false;
        if (!GetConsoleMode(m_hIn, &m_origInMode)) return false;
        m_isConsoleInput = true;

        // Enable VT100 escapes & Mouse Input. If VT cannot be enabled, abort to avoid garbled output.
        DWORD newOutMode = m_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        DWORD newInMode = (m_origInMode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;

        if (!SetConsoleMode(m_hOut, newOutMode)) return false;
        if (!SetConsoleMode(m_hIn, newInMode)) return false;

        _setmode(_fileno(stdout), _O_U16TEXT);

        // Switch to alternate screen and hide cursor for full-screen TUI behavior.
        std::wcout << L"\033[?1049h\033[?25l\033[H\033[?1000h\033[?1006h";
        UpdateDimensions();
        m_initialized = true;
        return true;
    }

    ~ConsoleRenderer() {
        if (!m_initialized) return;

        // Restore screen/cursor and console modes.
        std::wcout << Color::RESET << L"\033[?1000l\033[?1006l\033[?25h\033[?1049l";
        SetConsoleMode(m_hOut, m_origOutMode);
        SetConsoleMode(m_hIn, m_origInMode);
    }

    void UpdateDimensions() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(m_hOut, &csbi)) {
            m_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            m_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
    }

    SHORT GetWidth() const { return m_width; }
    SHORT GetHeight() const { return m_height; }
    bool IsConsoleInput() const { return m_isConsoleInput; }

    void ClearBuffer() {
        m_buffer.clear();
        // Keep a reusable frame buffer to reduce allocations in the hot render loop.
        size_t targetChars = static_cast<size_t>((std::max<SHORT>)(m_width, 80))
                           * static_cast<size_t>((std::max<SHORT>)(m_height, 25))
                           * 2;
        if (m_buffer.capacity() < targetChars) {
            m_buffer.reserve(targetChars);
        }
        m_buffer += L"\033[2J\033[H"; // Clear screen and move cursor to top-left
    }

    void Write(const std::wstring& str) {
        m_buffer += str;
    }

    void Flush() {
        if (m_buffer.empty()) return;
        DWORD written = 0;
        WriteConsoleW(m_hOut, m_buffer.c_str(), static_cast<DWORD>(m_buffer.length()), &written, NULL);
    }

    std::wstring DrawBar(int width, double percent, const wchar_t* color) {
        int fill = static_cast<int>((percent / 100.0) * width);
        fill = (std::max)(0, (std::min)(width, fill));

        std::wstring bar;
        bar.reserve(static_cast<size_t>(width) + 16);
        bar += L"[";
        bar += color;
        bar.append(static_cast<size_t>(fill), L'|');
        bar += Color::RESET;
        bar.append(static_cast<size_t>(width - fill), L' ');
        bar += L"]";
        return bar;
    }
};

