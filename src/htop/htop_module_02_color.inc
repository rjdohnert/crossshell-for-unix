// ============================================================================
// ANSI COLOR CODES & THEME ENGINE
// ============================================================================
namespace Color {
    constexpr wchar_t RESET[]            = L"\033[0m";
    constexpr wchar_t BOLD[]             = L"\033[1m";
    constexpr wchar_t DIM[]              = L"\033[2m";
    
    // Foreground
    constexpr wchar_t BLACK[]            = L"\033[30m";
    constexpr wchar_t RED[]              = L"\033[31m";
    constexpr wchar_t GREEN[]            = L"\033[32m";
    constexpr wchar_t YELLOW[]           = L"\033[33m";
    constexpr wchar_t BLUE[]             = L"\033[34m";
    constexpr wchar_t MAGENTA[]          = L"\033[35m";
    constexpr wchar_t CYAN[]             = L"\033[36m";
    constexpr wchar_t WHITE[]            = L"\033[37m";
    
    constexpr wchar_t BRIGHT_GREEN[]     = L"\033[92m";
    constexpr wchar_t BRIGHT_YELLOW[]    = L"\033[93m";
    constexpr wchar_t BRIGHT_CYAN[]      = L"\033[96m";
    constexpr wchar_t BRIGHT_WHITE[]     = L"\033[97m";

    // Background
    constexpr wchar_t BG_DEFAULT[]       = L"\033[49m";
    constexpr wchar_t BG_RED[]           = L"\033[41m";
    constexpr wchar_t BG_BLUE[]          = L"\033[44m";
    constexpr wchar_t BG_CYAN[]          = L"\033[46m";
    constexpr wchar_t BG_GRAY[]          = L"\033[100m";
    constexpr wchar_t BG_SELECTION[]     = L"\033[44;37;1m"; // White on Blue
    constexpr wchar_t BG_TAGGED[]        = L"\033[43;30;1m"; // Black on Yellow
    constexpr wchar_t BG_HEADER[]        = L"\033[42;37;1m"; // White on Green
    constexpr wchar_t BG_FOOTER[]        = L"\033[40;37m";
}

