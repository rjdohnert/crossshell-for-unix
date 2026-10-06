#pragma once

#include "tcsh.hpp"

class TcshEngine {
private:
    std::string homeDir;
    std::string historyFile;
    std::string rcFile;
    std::vector<std::string> history;
    size_t historyIndex = 0;
    
    // TCSH Variables as Word Vectors (Lists/Arrays)
    std::map<std::string, std::vector<std::string>> variables;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::string> completions;
    std::map<std::string, std::string> keyBindings;
    std::map<std::string, std::string> trapHandlers;
    std::vector<std::string> positionalArgs;
    std::string scriptName;
    std::vector<std::string> dirStack;
    std::vector<Job> jobList;
    std::vector<ScheduledTask> scheduledTasks;
    std::string logFile;
    DWORD nextJobId = 1;
    bool running = true;
    bool hashEnabled = true;
    bool notifyJobs = true;
    // Tab completion state — reset on any non-Tab keypress
    std::vector<std::string> compCandidates;
    size_t   compIdx     = 0;
    std::string compStem;
    size_t   compStemPos = 0;
    bool     compActive  = false;
    std::string promptStr = "% ";
    size_t lastRedrawLineLength = 0;

    static constexpr size_t kMaxHistoryEntries = 1000;
    static constexpr size_t kMaxJobCount = 128;
    static constexpr size_t kMaxScriptDepth = 64;
    static constexpr size_t kMaxScriptLineLength = 8192;
    static constexpr size_t kMaxScriptLines = 20000;
    size_t scriptDepth = 0;

    enum class ScriptDirective {
        None,
        Break,
        Continue,
        Return
    };

    ScriptDirective scriptDirective = ScriptDirective::None;
    bool executingScript = false;

    struct ScriptFrameGuard {
        explicit ScriptFrameGuard(TcshEngine* shell) : shell(shell) {
            if (shell->scriptDepth >= kMaxScriptDepth) {
                exceeded = true;
                return;
            }
            shell->scriptDepth++;
            previousExecuting = shell->executingScript;
            shell->executingScript = true;
        }

        ~ScriptFrameGuard() {
            if (!exceeded && shell) {
                shell->scriptDepth--;
                shell->executingScript = previousExecuting;
            }
        }

        bool isDepthExceeded() const { return exceeded; }
        bool ok() const { return !exceeded; }

        TcshEngine* shell;
        bool exceeded = false;
        bool previousExecuting = false;
    };

    const std::vector<std::string>& builtins;

    // --- Core Lifecycle Helpers ---
    std::string getHomeDirectory();
    void initFiles();
    void loadHistory();
    void saveHistory();
    void loadRcFile();
    void generateDefaultRcFile();
    std::string resolveExecutable(const std::string& inputCmd, bool& foundOnDisk);

    // --- Parser Helpers ---
    std::string stripInlineComment(const std::string& line) const;
    std::vector<std::string> tokenize(const std::string& str, char delim = ' ') const;
    bool lexCommandLine(const std::string& line, std::vector<ParsedToken>& tokens) const;
    bool parseCommandLine(const std::string& line, ParsedPipeline& pipeline) const;
    std::string joinTokens(const std::vector<std::string>& tokens) const;
    std::vector<std::string> splitCommandSequence(const std::string& line, std::vector<std::string>& operators) const;

    // --- Builtins Helpers ---
    bool isBuiltinCommand(const std::string& command) const;
    std::string expandLeadingAlias(const std::string& line, const std::vector<std::string>& args) const;
    bool tryParseNonNegativeInt(const std::string& text, int& value, const std::string& commandName, const std::string& argumentName) const;
    bool shouldFallbackToCmd(const std::string& command) const;

    // --- Expansion & Variables Helpers ---
    std::string joinPositionalArgs() const;
    void setVariableList(const std::string& key, const std::vector<std::string>& val);
    std::string getVariableString(const std::string& key) const;
    bool isVariableSet(const std::string& token) const;
    void setScriptArguments(const std::string& filename, const std::vector<std::string>& args);
    bool shiftPositionalArguments(size_t count);
    std::string applyModifier(const std::string& str, char mod) const;
    std::string expandVariables(const std::string& str);
    std::string expandHistory(const std::string& line);
    std::vector<std::string> expandGlobs(const std::vector<std::string>& args) const;
    void evaluateArithmetic(const std::string& expr);
    std::string escapeArg(const std::string& arg);
    bool evalCondition(const std::string& expr);

    // --- Job Control Helpers ---
    void updateJobs();

    // --- Scripting Helpers ---
    void executeScriptLines(const std::vector<std::string>& lines);
    void executeSingleCommandLine(std::string line);

    // --- Terminal & Interactive Helpers ---
    std::string formatPrompt(const std::string& pattern) const;
    void redrawLine(HANDLE hConsole, COORD& startPos, const std::string& prompt, const std::string& buffer, size_t cursorIndex);

public:
    explicit TcshEngine(bool loadRc = true);
    ~TcshEngine();

    // --- Scripting & Execution Interface ---
    void run();
    bool runScript(const std::string& filename, const std::vector<std::string>& args = {});
    int executeCommandString(const std::string& command, const std::string& name = "", const std::vector<std::string>& args = {});
    void executeCommandLine(std::string line);
    bool executeBuiltin(const std::vector<std::string>& args);
    void executePipeline(const ParsedPipeline& pipeline);

    // --- Info & Display ---
    void displayHelp();
    void displayVersion();

    // --- Interactive Editing & Completion ---
    std::string readLineWithEditing();
    void handleTabCompletion(std::string& currentBuffer, size_t& cursorIndex);

    // --- Job Control Interface ---
    bool listJobs(const std::vector<std::string>& args);
    void bringJobToForeground(DWORD jobId);
    void sendJobToBackground(DWORD jobId);

    // --- String & Parsing Utilities ---
    std::string stripOuterQuotes(const std::string& str) const;
    std::string normalizePathSeparators(const std::string& path) const;
    std::string getVar(const std::string& key) const { return getVariableString(key); }
    int getStatus() const { return std::atoi(getVariableString("status").c_str()); }
};
