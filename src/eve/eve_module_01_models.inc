static std::atomic<bool> g_consoleCtrlRequested{false};

static BOOL WINAPI ConsoleCtrlHandler(DWORD controlType) {
    if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT ||
        controlType == CTRL_CLOSE_EVENT || controlType == CTRL_LOGOFF_EVENT ||
        controlType == CTRL_SHUTDOWN_EVENT) {
        g_consoleCtrlRequested.store(true, std::memory_order_relaxed);

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr) {
            INPUT_RECORD ir = { 0 };
            ir.EventType = FOCUS_EVENT;
            ir.Event.FocusEvent.bSetFocus = TRUE;
            DWORD written = 0;
            WriteConsoleInputW(hIn, &ir, 1, &written);
        }
        return TRUE;
    }
    return FALSE;
}

enum class Lang {
    Plain, C_Cpp, Java, Python, Swift, Kotlin, Fortran,
    Cobol, Pascal, JavaScript, Basic, PowerShell, Batch, Shell
};

enum class EveTheme { Default, Black, Orange, Green, Red, Blue, Pink };

enum class SyntaxTokenType : uint8_t {
    Default, Keyword, Type, String, Comment, Number, Directive
};

struct HighlightState {
    char commentTerminator = '\0';
    char quote = '\0';
    bool operator==(const HighlightState& other) const {
        return commentTerminator == other.commentTerminator && quote == other.quote;
    }
};

