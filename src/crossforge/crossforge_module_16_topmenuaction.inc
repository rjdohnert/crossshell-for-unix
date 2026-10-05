    enum class TopMenuAction {
        None,
        File,
        Edit,
        Search,
        Run,
        Compile,
        Git,
        Debug,
        Options,
        Window,
        Help
    };

    ThemeScheme m_currentTheme = Theme_BorlandBlue;
    ThemePalette m_palette;
    LanguageRegistry m_langRegistry;
    TopMenuAction m_pressedTopMenuAction = TopMenuAction::None;
    std::string m_statusMessage;
    std::string m_clipboardText;
    std::string m_lastToolTitle;
    std::string m_lastToolCommand;
    std::string m_lastToolOutput;
    bool m_hasLastToolOutput = false;
    std::string m_lastDebugTitle;
    std::string m_lastDebugCommand;
    std::string m_lastDebugOutput;
    bool m_hasLastDebugOutput = false;
    bool m_selectAllActive = false;
    bool m_needsRender = true;
    std::string m_lastSearchTerm;
    std::vector<std::string> m_recentFiles;
    size_t m_recentIndex = 0;
