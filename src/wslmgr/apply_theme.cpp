#include "wslmgr_common.hpp"

void ApplyTheme(ThemeName theme) {
    const auto palette = MakePalette(theme);
    Color::RESET = "\033[0m";
    Color::BOLD = "\033[1m";
    Color::RED = "\033[91m";
    Color::GREEN = "\033[92m";
    Color::YELLOW = "\033[93m";
    Color::BLUE = "\033[94m";
    Color::MAGENTA = "\033[95m";
    Color::CYAN = "\033[96m";
    Color::WHITE = "\033[97m";
    Color::HEADER_TEXT = palette.headerText;
    Color::BG_DARK = palette.tabBg;
    Color::BG_BLUE = palette.headerBg;
    Color::BG_HIGHLIGHT = palette.highlight;
    Color::CLEAR_LINE = "\033[K";

    // Keep a stable accent color for themed rendering.
    (void)palette;
}
