#include "clipboard.hpp"
#include "key_event_encoder.hpp"
#include "layout_node.hpp"
#include "shell_command.hpp"
#include "string_encoding.hpp"
#include "tmux_app.hpp"
#include "tmux_config.hpp"
#include "tmux_help.hpp"
#include "wmux_engine.hpp"

int runTmux(int argc, char* argv[]) {
    std::vector<std::string> cmdArgs;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") { PrintHelp(); return 0; }
        if (arg == "-v" || arg == "--version") {
            std::cout << "tmux version 3.1 Licensed under the BSD 3-Clause License\n";
            return 0;
        }
        cmdArgs.push_back(arg);
    }

    WmuxEngine mux;
    EnsureTmuxRcExists();
    LoadTmuxRc(mux);
    if (!cmdArgs.empty()) {
        std::string combined;
        for (size_t i = 0; i < cmdArgs.size(); ++i) {
            if (i > 0) combined += " ";
            combined += cmdArgs[i];
        }
        mux.defaultShell = StringToWString(combined);
    }

    if (!mux.InitConsole()) {
        std::cerr << "Fatal: Failed to initialize Windows ConPTY.\n";
        return 1;
    }

    // Event-Driven Render Thread
    std::thread renderThread([&mux]() {
        while (mux.isRunning) {
            std::unique_lock<std::mutex> lock(mux.dirtyMtx);
            mux.cvDirty.wait_for(lock, std::chrono::milliseconds(1000), [&mux]() {
                return mux.isDirty.load() || !mux.isRunning.load();
            });
            if (!mux.isRunning) break;
            mux.isDirty = false;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            mux.RenderFrame();
        }
    });

    // Main Input Loop
    INPUT_RECORD rec;
    DWORD read;
    while (mux.isRunning && ReadConsoleInput(mux.hStdIn, &rec, 1, &read)) {
        std::unique_lock<std::recursive_mutex> lock(mux.stateMtx);
        if (!mux.isRunning || mux.windows.empty()) break;
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            mux.UpdateSize();
            mux.RecalculateLayout();
            continue;
        }

        if (rec.EventType == MOUSE_EVENT) {
            mux.HandleMouseEvent(rec.Event.MouseEvent);
            continue;
        }

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;

        KEY_EVENT_RECORD ker = rec.Event.KeyEvent;
        char ch = ker.uChar.AsciiChar;

        if (mux.showHelp) {
            mux.showHelp = false;
            mux.MarkDirty();
            continue;
        }

        if (mux.inPromptMode) {
            if (ker.wVirtualKeyCode == VK_RETURN) {
                mux.inPromptMode = false;
                if (mux.promptAction == WmuxEngine::PromptAction::RenameWindow && !mux.promptInput.empty()) {
                    mux.windows[mux.activeWindowIdx]->name = mux.promptInput;
                }
            } else if (ker.wVirtualKeyCode == VK_ESCAPE) { mux.inPromptMode = false; }
            else if (ker.wVirtualKeyCode == VK_BACK) { if (!mux.promptInput.empty()) mux.promptInput.pop_back(); }
            else if (ch >= 32 && ch <= 126) { mux.promptInput += ch; }
            mux.MarkDirty();
            continue;
        }

        if (mux.pendingKillPaneConfirm) {
            mux.ResolveKillPaneConfirm(ch == 'y' || ch == 'Y');
            continue;
        }

        // Global Alt Hotkeys & Arrow Key Navigation
        bool isAlt = (ker.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;

        if (mux.keys.themeCycle.Matches(ker)) {
            mux.CycleTheme();
            continue;
        }

        if (mux.keys.exitSession.Matches(ker)) {
            mux.isRunning = false;
            continue;
        }

        if (mux.keys.splitVertical.Matches(ker)) {
            mux.SplitActivePane(SplitType::Vertical);
            continue;
        }

        if (mux.keys.splitHorizontal.Matches(ker)) {
            mux.SplitActivePane(SplitType::Horizontal);
            continue;
        }

        if (mux.keys.newWindow.Matches(ker)) {
            mux.CreateWindowTab(ShellTabName(mux.defaultShell), mux.defaultShell);
            continue;
        }

        if (mux.keys.nextWindow.Matches(ker)) {
            mux.activeWindowIdx = (mux.activeWindowIdx + 1) % mux.windows.size();
            mux.RecalculateLayout();
            continue;
        }

        if (mux.keys.previousWindow.Matches(ker)) {
            mux.activeWindowIdx = (mux.activeWindowIdx - 1 + mux.windows.size()) % mux.windows.size();
            mux.RecalculateLayout();
            continue;
        }

        if (mux.keys.zoomPane.Matches(ker)) {
            mux.windows[mux.activeWindowIdx]->isZoomed = !mux.windows[mux.activeWindowIdx]->isZoomed;
            mux.RecalculateLayout();
            continue;
        }

        if (mux.keys.killPane.Matches(ker) || mux.keys.killPaneAlternate.Matches(ker)) {
            mux.RequestKillActivePaneConfirm();
            continue;
        }

        if (isAlt) {
            if (ker.wVirtualKeyCode == VK_LEFT || ker.wVirtualKeyCode == VK_RIGHT ||
                ker.wVirtualKeyCode == VK_UP || ker.wVirtualKeyCode == VK_DOWN) {
                mux.MoveFocus(ker.wVirtualKeyCode);
                continue;
            }
        }

        auto& activeP = mux.windows[mux.activeWindowIdx]->activePane;
        if (activeP && activeP->screen.scrollOffset > 0) {
            if (ch == 'q' || ker.wVirtualKeyCode == VK_ESCAPE) activeP->screen.scrollOffset = 0;
            else if (ker.wVirtualKeyCode == VK_PRIOR) activeP->screen.scrollOffset = std::min((int)activeP->screen.scrollback.size(), activeP->screen.scrollOffset + 10);
            else if (ker.wVirtualKeyCode == VK_NEXT)  activeP->screen.scrollOffset = std::max(0, activeP->screen.scrollOffset - 10);
            else if (ker.wVirtualKeyCode == VK_UP)    activeP->screen.scrollOffset = std::min((int)activeP->screen.scrollback.size(), activeP->screen.scrollOffset + 1);
            else if (ker.wVirtualKeyCode == VK_DOWN)  activeP->screen.scrollOffset = std::max(0, activeP->screen.scrollOffset - 1);
            mux.MarkDirty();
            continue;
        }

        // Prefix key defaults to Ctrl+B and can be changed in .tmuxrc.
        if (mux.keys.prefixKey.Matches(ker)) {
            mux.inPrefixMode = true;
            mux.MarkDirty();
            continue;
        }

        if (mux.inPrefixMode) {
            if (ker.wVirtualKeyCode == VK_LEFT || ker.wVirtualKeyCode == VK_RIGHT ||
                ker.wVirtualKeyCode == VK_UP || ker.wVirtualKeyCode == VK_DOWN) {
                mux.inPrefixMode = false;
                mux.MoveFocus(ker.wVirtualKeyCode);
                continue;
            }
            mux.inPrefixMode = false;
            switch (ch) {
            case '%': mux.SplitActivePane(SplitType::Vertical); break;
            case '"': mux.SplitActivePane(SplitType::Horizontal); break;
            case 'o': {
                auto panes = mux.windows[mux.activeWindowIdx]->GetPanes();
                for (size_t i = 0; i < panes.size(); ++i) {
                    if (panes[i] == mux.windows[mux.activeWindowIdx]->activePane) {
                        mux.windows[mux.activeWindowIdx]->activePane = panes[(i + 1) % panes.size()];
                        break;
                    }
                }
                mux.MarkDirty();
                break;
            }
            case 'z':
                mux.windows[mux.activeWindowIdx]->isZoomed = !mux.windows[mux.activeWindowIdx]->isZoomed;
                mux.RecalculateLayout();
                break;
            case 'x':
                mux.KillActivePane();
                break;
            case 'c': mux.CreateWindowTab(ShellTabName(mux.defaultShell), mux.defaultShell); break;
            case 'n': mux.activeWindowIdx = (mux.activeWindowIdx + 1) % mux.windows.size(); mux.RecalculateLayout(); break;
            case 'p': mux.activeWindowIdx = (mux.activeWindowIdx - 1 + mux.windows.size()) % mux.windows.size(); mux.RecalculateLayout(); break;
            case '&':
                mux.windows.erase(mux.windows.begin() + mux.activeWindowIdx);
                if (mux.windows.empty()) mux.isRunning = false;
                else { mux.activeWindowIdx = std::min(mux.activeWindowIdx, mux.windows.size() - 1); mux.RecalculateLayout(); }
                break;
            case ',':
                mux.inPromptMode = true;
                mux.promptAction = WmuxEngine::PromptAction::RenameWindow;
                mux.promptQuery = "(rename-window) ";
                mux.promptInput = mux.windows[mux.activeWindowIdx]->name;
                mux.MarkDirty();
                break;
            case '[':
                if (activeP) activeP->screen.scrollOffset = std::min(1, (int)activeP->screen.scrollback.size());
                mux.MarkDirty();
                break;
            case ']': {
                std::wstring clip = GetOSClipboard();
                if (!clip.empty() && activeP) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, clip.c_str(), -1, NULL, 0, NULL, NULL);
                    if (len > 0) {
                        std::vector<char> buf(len);
                        WideCharToMultiByte(CP_UTF8, 0, clip.c_str(), -1, buf.data(), len, NULL, NULL);
                        activeP->SendInput(std::string(buf.data()));
                    }
                }
                break;
            }
            case '?': mux.showHelp = true; mux.MarkDirty(); break;
            case 'd': mux.isRunning = false; break;
            default:
                if (ch >= '0' && ch <= '9') {
                    size_t idx = ch - '0';
                    if (idx < mux.windows.size()) { mux.activeWindowIdx = idx; mux.RecalculateLayout(); }
                }
                break;
            }
            continue;
        }

        if (activeP) {
            std::string vtSeq = EncodeKeyEvent(ker);
            if (!vtSeq.empty()) activeP->SendInput(vtSeq);
        }
    }

    mux.isRunning = false;
    mux.MarkDirty();
    if (renderThread.joinable()) renderThread.join();
    mux.RestoreConsole();
    return 0;
}
