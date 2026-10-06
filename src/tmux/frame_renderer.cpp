#include "cell.hpp"
#include "color.hpp"
#include "rect.hpp"
#include "ui_theme.hpp"
#include "wmux_engine.hpp"

void WmuxEngine::RenderFrame() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        ReapDeadPanes();
        if (windows.empty()) return;
        std::vector<std::vector<Cell>> frame(termH, std::vector<Cell>(termW, Cell{ L' ', Color::DefaultFg(), Color::DefaultBg() }));
        auto& win = windows[activeWindowIdx];
        auto panes = win->GetPanes();

        // 1. Draw Panes
        for (auto& p : panes) {
            if (win->isZoomed && p != win->activePane) continue;
            Rect b = p->bounds;

            // Draw Virtual Screen Grid onto Composite Buffer
            std::lock_guard<std::mutex> lock(p->screen.mtx);
            int scroll = p->screen.scrollOffset;
            for (int y = 0; y < p->screen.height; ++y) {
                int screenY = b.y + y;
                if (screenY >= b.y + b.height || screenY >= termH) break;

                const std::vector<Cell>* src = nullptr;
                if (scroll > 0) {
                    int idx = (int)p->screen.scrollback.size() - scroll + y;
                    if (idx >= 0 && idx < (int)p->screen.scrollback.size()) src = &p->screen.scrollback[idx];
                }
                if (!src && y < (int)p->screen.ActiveGrid().size()) src = &p->screen.ActiveGrid()[y];

                if (src) {
                    for (int x = 0; x < p->screen.width; ++x) {
                        int screenX = b.x + x;
                        if (screenX >= b.x + b.width || screenX >= termW) break;
                        if (x < (int)src->size()) frame[screenY][screenX] = (*src)[x];
                    }
                }
            }
        }

        // 2. Draw Dividers & Smooth Borders
        if (!win->isZoomed) {
            DrawLayoutDividers(win->rootLayout, frame, win->activePane);
            SmoothBorders(frame);
        }

        // 2. Tmux Status Bar
        int sbY = termH - 1;
        const UiTheme& theme = Theme();
        Color sbBg = inPrefixMode ? theme.statusPrefixBg : theme.statusBg;
        Color sbFg = theme.statusFg;

        for (int x = 0; x < termW; ++x) frame[sbY][x] = Cell{ L' ', sbFg, sbBg };

        if (pendingKillPaneConfirm) {
            std::ostringstream pr;
            pr << "kill-pane #" << (pendingKillPaneTarget ? pendingKillPaneTarget->id : -1) << "? (y/n) ";
            std::string prStr = pr.str();
            for (size_t i = 0; i < prStr.size() && (int)i < termW; ++i) {
                frame[sbY][i] = Cell{ (wchar_t)prStr[i], theme.helpFg, theme.promptBg, true };
            }
        } else if (inPromptMode) {
            std::string pr = promptQuery + promptInput + "_";
            for (size_t i = 0; i < pr.size() && (int)i < termW; ++i) {
                frame[sbY][i] = Cell{ (wchar_t)pr[i], theme.helpFg, theme.promptBg, true };
            }
        } else {
            std::ostringstream left;
            if (inPrefixMode) left << "[PREFIX] ";
            left << "[" << activeWindowIdx << "] ";

            for (size_t i = 0; i < windows.size(); ++i) {
                left << i << ":" << windows[i]->name << (i == activeWindowIdx ? "* " : "  ");
            }
            if (win->activePane->screen.scrollOffset > 0) left << "[COPY: " << win->activePane->screen.scrollOffset << "] ";
            if (win->isZoomed) left << "[Z] ";

            std::string lStr = left.str();
            for (size_t i = 0; i < lStr.size() && (int)i < termW; ++i) {
                frame[sbY][i] = Cell{ (wchar_t)lStr[i], sbFg, sbBg, lStr[i] == '*' };
            }

            char host[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD hLen = sizeof(host);
            GetComputerNameA(host, &hLen);
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm;
            localtime_s(&tm, &now);
            std::ostringstream right;
            right << "\"" << host << "\" " << std::put_time(&tm, "%H:%M:%S %d-%b-%y ");

            std::string rStr = right.str();
            int rStart = termW - (int)rStr.size();
            for (size_t i = 0; i < rStr.size(); ++i) {
                if (rStart + (int)i >= 0 && rStart + (int)i < termW) {
                    frame[sbY][rStart + i] = Cell{ (wchar_t)rStr[i], sbFg, sbBg };
                }
            }
        }

        // 3. Interactive Help Overlay
        if (showHelp) {
            int hw = 70, hh = 21;
            int hx = (termW - hw) / 2, hy = (termH - hh) / 2;
            const char* helpLines[] = {
                "                    tmux v3.1 - Terminal Multiplexer            ",
                "------------------------------------------------------------------",
                " GLOBAL ALT HOTKEYS (Fast, No Prefix Needed):                     ",
                "  Alt+V / Alt+H      Split vertically / horizontally               ",
                "  Alt+Arrow Keys     Navigate between adjacent panes               ",
                "  Alt+T              Create new window tab                         ",
                "  Alt+N / Alt+P      Switch to next / previous tab                 ",
                "  Alt+Z              Toggle zoom on active pane                    ",
                "  Alt+W / Alt+Q      Kill pane (y/n) | Ctrl+Alt+Q Exit session      ",
                "  Ctrl+Alt+T         Cycle blue/black/orange/silver/pink/yellow    ",
                "------------------------------------------------------------------",
                " PREFIX HOTKEYS (Ctrl+b, then release and press command key):      ",
                "  Ctrl+b % / \"       Split vertically / horizontally               ",
                "  Ctrl+b Arrow       Navigate focus between panes                  ",
                "  Ctrl+b c / &       Create new window tab / Kill tab              ",
                "  Ctrl+b n / p       Switch to next / previous tab                 ",
                "  Ctrl+b z / x       Toggle zoom pane / Kill pane                  ",
                "  Ctrl+b [ / ]       Scrollback mode / Paste OS Clipboard          ",
                "  Ctrl+b ? / d       Toggle this help overlay / Exit tmux          ",
                "------------------------------------------------------------------",
                " [Mouse]: Click pane to focus | Drag border to resize splits       "
            };

            for (int y = 0; y < hh; ++y) {
                for (int x = 0; x < hw; ++x) {
                    int fx = hx + x, fy = hy + y;
                    if (fx >= 0 && fx < termW && fy >= 0 && fy < termH) {
                        wchar_t ch = L' ';
                        if (y < hh && x < (int)strlen(helpLines[y])) ch = (wchar_t)(uint8_t)helpLines[y][x];
                        frame[fy][fx] = Cell{ ch, theme.helpFg, theme.helpBg, true };
                    }
                }
            }
        }

        // 4. Output Stream Generation
        std::wostringstream out;
        out << L"\x1b[H\x1b[0m";
        Color lastFg = Color::DefaultFg(), lastBg = Color::DefaultBg();
        bool lastBold = false, lastUnderline = false, lastInverse = false;

        for (int y = 0; y < termH; ++y) {
            for (int x = 0; x < termW; ++x) {
                const auto& c = frame[y][x];
                if (c.width == 0) continue; // Skip wide trailer cell! Physical cursor already advanced 2 columns

                if (c.fg != lastFg || c.bg != lastBg || c.bold != lastBold || c.underline != lastUnderline || c.inverse != lastInverse) {
                    out << L"\x1b[0";
                    if (c.bold) out << L";1";
                    if (c.underline) out << L";4";
                    if (c.inverse) out << L";7";

                    if (!c.fg.isDefault) {
                        if (c.fg.isIndexed) out << L";38;5;" << (int)c.fg.index;
                        else out << L";38;2;" << (int)c.fg.r << L";" << (int)c.fg.g << L";" << (int)c.fg.b;
                    }
                    if (!c.bg.isDefault) {
                        if (c.bg.isIndexed) out << L";48;5;" << (int)c.bg.index;
                        else out << L";48;2;" << (int)c.bg.r << L";" << (int)c.bg.g << L";" << (int)c.bg.b;
                    }
                    out << L"m";

                    lastFg = c.fg; lastBg = c.bg;
                    lastBold = c.bold; lastUnderline = c.underline; lastInverse = c.inverse;
                }
                out << c.ch;
            }
            if (y < termH - 1) out << L"\r\n";
        }

        if (!showHelp && win->activePane && win->activePane->screen.cursorVisible) {
            int cx = win->activePane->bounds.x + win->activePane->screen.cursorX;
            int cy = win->activePane->bounds.y + win->activePane->screen.cursorY;
            out << L"\x1b[" << (cy + 1) << L";" << (cx + 1) << L"H" << VT_SHOW_CURSOR;
        } else {
            out << VT_HIDE_CURSOR;
        }

        std::wstring s = out.str();
        DWORD written;
        WriteConsoleW(hStdOut, s.c_str(), (DWORD)s.length(), &written, NULL);
    }
