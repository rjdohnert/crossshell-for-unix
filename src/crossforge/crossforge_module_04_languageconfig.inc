struct LanguageConfig {
    SupportedLanguage Lang;
    std::string Name;
    std::vector<std::string> Extensions;
    std::vector<std::string> Keywords;
    std::string CompilerExe;
    std::string DebuggerExe;
    std::string CompileCmdTemplate;
    std::string DebugCmdTemplate;
};

