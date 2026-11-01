#ifndef CROSSFORGE_ENGINE_H
#define CROSSFORGE_ENGINE_H

#include "crossforge.h"
#include "supported_language.h"
#include "theme_palette.h"
#include "theme_manager.h"
#include "language_registry.h"
#include "document.h"
#include "menu_entry.h"
#include "menu_command.h"
#include "top_menu_action.h"
#include "diagnostic.h"
#include "window_layout.h"

class CrossForgeEngine {
private:
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

    std::vector<Diagnostic> m_diagnostics;
    size_t m_diagnosticIndex = 0;

    WindowLayout m_windowLayout = WindowLayout::Split;

    bool m_running = true;

    // --- Async tool execution ---
    // External build/run/git/debug commands run on a worker thread so the UI
    // stays responsive. The worker only runs ProcessRunner and captures output;
    // all UI state (last-tool fields, diagnostics, status) is applied on the
    // main thread when it drains m_completedTools from the event loop.
    struct ToolResult {
        std::string Title;
        std::string Command;
        std::string Output;
        bool Ok = false;
        bool IsDebug = false;
    };
    std::thread m_toolThread;
    std::mutex m_toolMutex;
    std::deque<ToolResult> m_completedTools;
    std::atomic<bool> m_toolRunning{ false };
    std::string m_pendingToolTitle;
    std::string m_pendingDebugNote; // appended to debug-probe output on completion

public:
    CrossForgeEngine();
    ~CrossForgeEngine();
    void Run();

private:
    static fs::path GetUserProjectPath();

    static fs::path GetLegacyUserProjectPath();

    static fs::path GetUserHomePath();

    static std::string GetLocalTimestamp();

    static void LogCompilerOutput(const std::string& title, const std::string& command,
                                  const std::string& output, bool ok);

    void RememberRecentFile(const std::string& path);

    void LoadUserProject();

    void SaveUserProject() const;

    static std::string Utf8FromWideChar(wchar_t wch);

    static size_t PrevUtf8CharStart(const std::string& s, size_t pos);

    static size_t NextUtf8CharEnd(const std::string& s, size_t pos);

    static void PopLastUtf8Codepoint(std::string& s);

    static uint32_t DecodeUtf8Codepoint(const std::string& s, size_t pos, int& len);

    static int CodepointDisplayWidth(uint32_t cp);

    static constexpr int kTabWidth = 4;

    static int Utf8DisplayWidth(const std::string& s);

    static int Utf8DisplayWidth(const std::string& s, size_t byteCount);

    static int ByteOffsetToDisplayCol(const std::string& s, int byteOffset);

    static int DisplayColToByteOffset(const std::string& s, int col);

    static bool IsLikelyCompilerToolDirectory(const fs::path& dir);

    static bool IsSafePathSegment(const std::string& value);

    void EnsureVisualStudioBuildToolsInPath();

    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

    void CleanupConsoleOnExit();

    void ApplyEditorConsoleModes();

    bool UpdateBufferSize();

    void PutChar(int x, int y, wchar_t ch, WORD attr);

    void DrawString(int x, int y, const std::string& str, WORD attr);

    bool SetCursorVisible(bool visible);

    void SetCursorPositionVT(int x, int y);

    void ClearBuffer(WORD attr);

    bool FillInputBuffer();

    int ReadByte();

    int PeekByte();

    static void MakeKeyEvent(INPUT_RECORD& ir, WORD vk, wchar_t ch, DWORD ctrlState = 0);

    bool ParseCSI(INPUT_RECORD& ir);

    void ParseSgrMouse(INPUT_RECORD& ir, const std::string& params, bool pressed);

    bool DecodeInputEvent(INPUT_RECORD& ir);

    bool ReadInputEvent(INPUT_RECORD& ir);

    static int ConsoleColorToAnsiIndex(WORD colorNibble);

    static void AppendAttrAsSGR(std::string& out, WORD attr);

    static void AppendUtf8(std::string& out, wchar_t wc);

    void FlushBuffer();

    void RenderScreen();

    void RenderMenuBar();

    bool TryGetTopMenuRegionX(TopMenuAction action, int& startX, int& endX) const;

    void EnsureDocumentInvariants(const std::shared_ptr<Document>& doc);

    void SetStatus(const std::string& text);

    static bool SetSystemClipboardText(const std::string& text);

    static bool GetSystemClipboardText(std::string& text);

    void UndoActiveDocument();

    void RedoActiveDocument();

    static bool IsCtrlPressed(DWORD state);

    static bool IsAltPressed(DWORD state);

    static bool IsShiftPressed(DWORD state);

    bool HandleMenuAccelerator(WORD vk, DWORD state);

    void NewDocument();

    void SaveActiveDocument();

    void SaveActiveDocumentAs(const std::string& path);

    void CycleTheme();

    bool GetDocWindowFrame(int docIndex, int& topY, int& botY) const;

    int GetDocumentViewHeight(int docIndex) const;

    void EnsureCursorVisible(const std::shared_ptr<Document>& doc, int docIndex);

    void OpenDocumentFromPrompt();

    bool ShowPathNavigatorDialog(const std::string& title, bool mustExistFile, bool openMode, std::string& ioPath, bool selectDirectory = false);

    bool ShowInputDialog(const std::string& title, const std::string& prompt, std::string& value);

    std::string PromptLine(const std::string& prompt, const std::string& title = "Input");

    static void ReplaceAllTokens(std::string& text, const std::string& token, const std::string& value);

    static std::string TrimWhitespace(const std::string& input);

    static std::string StripWrappingQuotes(std::string input);

    static bool NormalizePathString(const std::string& rawPath, std::string& normalizedOut);

    bool EnsureActiveDocumentPath(std::shared_ptr<Document>& doc);

    std::vector<std::string> SplitOutputLines(const std::string& text);

    void ShowCommandOutputDialog(const std::string& title, const std::string& command, const std::string& output, bool ok);

    void ShowBuildResultDialog(bool ok);

    void StartToolProcess(const std::string& title, const std::string& command, bool isDebug);
    void DrainCompletedTools();
    void ApplyToolResult(const ToolResult& result);

    void ShowLastCompilerOutput();

    void ParseDiagnostics(const std::string& output);

    void GoToNextDiagnostic();

    void AddCompilerPathOption();

    std::string FindToolInPath(const std::string& exeName) const;

    std::string DetectDebuggerForLanguage(SupportedLanguage lang) const;

    std::string DetectLinkerForLanguage(SupportedLanguage lang) const;

    void ShowDebuggerDetectDialog();

    void RunDebugForActiveTarget();

    void ShowLastDebugOutput();

    void RunLinkerForActiveTarget();

    void BuildActiveTarget();

    void RunActiveTarget();

    void TestActiveTarget();

    fs::path GitWorkDir() const;

    void RunGitCommand(const std::string& title, const std::string& args);

    void GitStatus();

    void GitPull();

    void GitPush();

    void GitLog();

    void GitDiff();

    void GitAddAll();

    void GitCommit();

    bool ConfirmDocumentClose(int index);

    void RequestExit();

    void CloseActiveDocument();

    void CloseDocumentAtIndex(int index);

    void InsertLineBelow();

    void DeleteCurrentLine();

    void DuplicateCurrentLine();

    void CopySelectionOrLine();

    void CutSelectionOrLine();

    void PasteClipboardAtCursor();

    void SelectAllBuffer();

    void FindTextPrompt();

    void FindNext(bool reverse);

    void ReplaceTextPrompt();

    void ShowDiagnosticsDialog();

    std::vector<MenuEntry> GetMenuEntries(TopMenuAction menu) const;

    void ExecuteMenuCommand(MenuCommand cmd);

    void ShowMenuPopup(TopMenuAction menu);

    int HitTestWindowCloseButton(int x, int y) const;

    int HitTestTabStrip(int x, int y) const;

    TopMenuAction HitTestTopMenu(int x) const;

    void HandleTopMenuAction(TopMenuAction action);

    void RenderEditorWindows();

    void RenderTabStrip();

    void CycleWindowLayout();

    void RenderSingleWindow(std::shared_ptr<Document> doc, bool isActive, int topY, int botY);

    void ScanLineState(const std::string& line, HighlightState& state);

    void RenderHighlightedLine(int startX, int y, const std::string& line, const LanguageConfig& cfg, int maxLen, HighlightState& state, int lineStartCol = 0);

    void RenderStatusBar();

    std::string GetWindowsVersionText() const;

    void ShowHelpDialog();

    void ShowAboutDialog();

    void ShowCompilerDetectDialog();

    void LaunchInteractiveTerminal(const std::string& title, const std::string& commandLine);

    void LaunchTerminalIntegration();

    void LaunchWslTerminal();

    void LaunchSshTerminal();

    void ProcessInput();
};

#endif // CROSSFORGE_ENGINE_H
