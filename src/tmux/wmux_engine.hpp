#pragma once

#include "cell.hpp"
#include "helper_keys.hpp"
#include "layout_node.hpp"
#include "pane.hpp"
#include "shell_command.hpp"
#include "tmux.hpp"
#include "ui_theme.hpp"
#include "window.hpp"

class WmuxEngine {
public:
    HANDLE hStdIn, hStdOut;
    DWORD origInMode = 0, origOutMode = 0;
    std::vector<std::shared_ptr<Window>> windows;
    size_t activeWindowIdx = 0;
    int nextPaneId = 0, nextWindowId = 0;
    int termW = 80, termH = 24;

    std::atomic<bool> isRunning{ true };
    std::atomic<bool> inPrefixMode{ false };
    std::atomic<bool> showHelp{ false };
    std::atomic<bool> inPromptMode{ false };
    std::string promptQuery = "", promptInput = "";
    enum class PromptAction { RenameWindow } promptAction;

    std::atomic<bool> pendingKillPaneConfirm{ false };
    std::shared_ptr<Pane> pendingKillPaneTarget;

    std::shared_ptr<LayoutNode> draggingBorderNode = nullptr;

    std::atomic<bool> isDirty{ true };
    std::condition_variable cvDirty;
    std::mutex dirtyMtx;
    std::recursive_mutex stateMtx;
    std::wstring defaultShell = DetectDefaultShell();
    size_t activeThemeIdx = 0;
    HelperKeys keys;

    WmuxEngine();

    void MarkDirty();

    const UiTheme& Theme() const;

    bool SetTheme(const std::string& name);

    void CycleTheme();

    bool InitConsole();

    void RestoreConsole();

    void UpdateSize();

    void CreateWindowTab(const std::string& name, const std::wstring& shellCmd);

    void RecalculateLayout();

    void SplitActivePane(SplitType type);

    void KillPane(std::shared_ptr<Pane> p);

    void KillActivePane();

    // Arms a y/n confirmation instead of killing the active pane immediately.
    void RequestKillActivePaneConfirm();

    void ResolveKillPaneConfirm(bool confirmed);

    void ReapDeadPanes();

    bool IsBorderChar(wchar_t ch);

    void SmoothBorders(std::vector<std::vector<Cell>>& frame);

    void DrawLayoutDividers(std::shared_ptr<LayoutNode> node, std::vector<std::vector<Cell>>& frame, std::shared_ptr<Pane> activePane);

    void RenderFrame();

    void HandleMouseEvent(const MOUSE_EVENT_RECORD& mer);

    void MoveFocus(int dirKey);
};
