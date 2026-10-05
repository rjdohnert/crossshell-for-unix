class ThemeManager {
public:
    static ThemePalette GetTheme(ThemeScheme scheme) {
        ThemePalette p{};
        switch (scheme) {
        case Theme_Monokai:
            p.MenuAttr          = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            p.MenuHotkeyAttr    = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.DesktopAttr       = 0;
            p.WinBorderActive   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            p.WinTitleAttr      = BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.EditorBg          = 0;
            p.TextDefault       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.KeywordAttr       = FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.StringAttr        = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.CommentAttr       = FOREGROUND_GREEN;
            p.NumberAttr        = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            p.StatusBarAttr     = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            break;

        case Theme_Matrix:
            p.MenuAttr          = BACKGROUND_GREEN;
            p.MenuHotkeyAttr    = BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.DesktopAttr       = 0;
            p.WinBorderActive   = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = FOREGROUND_GREEN;
            p.WinTitleAttr      = BACKGROUND_GREEN | FOREGROUND_INTENSITY;
            p.EditorBg          = 0;
            p.TextDefault       = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.KeywordAttr       = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.StringAttr        = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.CommentAttr       = FOREGROUND_GREEN;
            p.NumberAttr        = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = FOREGROUND_GREEN;
            p.StatusBarAttr     = BACKGROUND_GREEN;
            break;

        case Theme_BorlandBlue:
        default:
            // Default palette with blue editor background.
            p.MenuAttr          = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.MenuHotkeyAttr    = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.DesktopAttr       = BACKGROUND_BLUE;
            p.WinBorderActive   = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinTitleAttr      = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.EditorBg          = BACKGROUND_BLUE;
            p.TextDefault       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.KeywordAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.StringAttr        = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.CommentAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.NumberAttr        = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.StatusBarAttr     = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            break;
        }
        return p;
    }
};

// ============================================================================
// LANGUAGE REGISTRY & SYNTAX HIGHLIGHTER
// ============================================================================

