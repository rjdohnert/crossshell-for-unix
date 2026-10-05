    struct Diagnostic {
        std::string FilePath;
        int Line = 0;
        std::string Message;
    };
    std::vector<Diagnostic> m_diagnostics;
    size_t m_diagnosticIndex = 0;

    // Window layout mode: Split tiles all open documents evenly, Maximized gives
    // the active document the full editor area, and Tabs shows a tab bar across
    // the top with the active document filling the rest.
