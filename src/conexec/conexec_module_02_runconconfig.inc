struct RunconConfig {
    IsolationLevel level = IsolationLevel::Normal;
    std::wstring command;
    std::wstring workingDirectory;
    bool stageCurrentDir = false;
    bool verbose = false;
};

