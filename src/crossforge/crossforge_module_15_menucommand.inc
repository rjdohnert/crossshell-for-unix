    enum class MenuCommand {
        None = 0,
        FileNew,
        FileOpen,
        FileSave,
        FileSaveAs,
        FileClose,
        FileExit,
        FileRecent,
        EditInsertLine,
        EditDeleteLine,
        EditDuplicateLine,
        EditUndo,
        EditRedo,
        EditCut,
        EditCopy,
        EditPaste,
        EditSelectAll,
        SearchFind,
        SearchFindNext,
        SearchFindPrevious,
        SearchReplace,
        RunTerminal,
        RunWslTerminal,
        RunSshTerminal,
        ViewCompilerOutput,
        ViewNextDiagnostic,
        CompileBuild,
        CompileRun,
        CompileTest,
        BuildDetect,
        GitStatus,
        GitAddAll,
        GitCommit,
        GitPull,
        GitPush,
        GitLog,
        GitDiff,
        DebugRunActive,
        DebugViewOutput,
        DebugDetect,
        OptionsNextTheme,
        OptionsAddCompilerPath,
        OptionsRunLinker,
        WindowNext,
        WindowNew,
        WindowLayoutToggle,
        HelpTopics,
        HelpDiagnostics,
        HelpAbout
    };

    HANDLE m_hStdOut;
    HANDLE m_hStdIn;
    DWORD m_defaultInputMode = 0;
    DWORD m_defaultOutputMode = 0;
    bool m_useClassicConsoleInput = false;
    CONSOLE_SCREEN_BUFFER_INFO m_csbi;
    int m_width;
    int m_height;

    std::vector<CHAR_INFO> m_screenBuffer;
    std::string m_inBuf;      // VT input accumulator (raw bytes from stdin)
    size_t m_inPos = 0;       // parse cursor into m_inBuf
    bool m_cursorVisible = true; // tracked for VT cursor show/hide
    std::vector<std::shared_ptr<Document>> m_documents;
    int m_activeDocIndex = 0;

