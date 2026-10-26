#include "wslmgr_common.hpp"

class App {
private:
    enum class InputKey {
        None,
        Character,
        Up,
        Down,
        Left,
        Right,
        Enter,
        Space,
        ProcessList,
        Lifecycle,
        FullDiagnostics
    };

    struct InputEvent {
        InputKey key = InputKey::None;
        int character = 0;
        bool mouseClick = false;
        bool doubleClick = false;
        int mouseWheel = 0;
        bool readError = false;
        COORD mousePosition = {};
    };

    int currentTab = 1;
    size_t selectedRow = 0;
    bool onlineCatalogLoaded = false;
    bool containersLoaded = false;
    std::string statusMsg = "Ready.";
    std::string statusColor = Color::GREEN;
    std::string diagnosticsStatus = "Not loaded. Press [F7] to open Full Diagnostics.";
    std::string diagnosticsVersion = "Not loaded.";
    std::vector<std::string> diagnosticsReportLines;
    std::string lifecycleDetails = "No lifecycle operation has been run.";
    std::vector<WslProcess> diagnosticsProcesses;
    size_t selectedProcess = 0;
    size_t diagnosticsProcessOffset = 0;
    bool showDiagnosticsProcesses = false;
    bool showFullDiagnostics = false;
    ThemeName currentTheme = ThemeName::Blue;
    GlyphStyle currentGlyphStyle = GlyphStyle::Unicode;
    size_t diagnosticsReportOffset = 0;
    size_t helpScrollOffset = 0;
    size_t catalogOffset = 0;
    size_t containerOffset = 0;
    std::vector<DistroInfo> cachedDistros;
    std::vector<OnlineDistro> cachedCatalog;
    std::vector<WslContainer> cachedContainers;
    std::vector<std::pair<SHORT, SHORT>> tabHitBoxes;

    SHORT ConsoleWidth() const {
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &info)) {
            return std::max<SHORT>(80, info.srWindow.Right - info.srWindow.Left + 1);
        }
        return 104;
    }

    SHORT ConsoleHeight() const {
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &info)) {
            return std::max<SHORT>(1, info.srWindow.Bottom - info.srWindow.Top + 1);
        }
        return 40;
    }

    size_t VisibleDiagnosticProcessRows() const {
        const SHORT reservedRows = 24;
        const SHORT availableRows = static_cast<SHORT>(ConsoleHeight() - reservedRows);
        return availableRows > 0 ? std::min<size_t>(10, static_cast<size_t>(availableRows)) : 0;
    }

    size_t VisibleHelpRows() const {
        const SHORT reservedRows = 24;
        const SHORT availableRows = static_cast<SHORT>(ConsoleHeight() - reservedRows);
        return availableRows > 0 ? std::min<size_t>(10, static_cast<size_t>(availableRows)) : 0;
    }

    size_t VisibleFullDiagnosticsRows() const {
        const SHORT reservedRows = 16;
        const SHORT availableRows = static_cast<SHORT>(ConsoleHeight() - reservedRows);
        return availableRows > 0 ? std::min<size_t>(10, static_cast<size_t>(availableRows)) : 0;
    }

    size_t VisibleCatalogRows() const {
        const SHORT reservedRows = 14;
        const SHORT availableRows = static_cast<SHORT>(ConsoleHeight() - reservedRows);
        return availableRows > 0 ? std::min<size_t>(5, static_cast<size_t>(availableRows)) : 0;
    }

    size_t VisibleContainerRows() const {
        const SHORT reservedRows = 14;
        const SHORT availableRows = static_cast<SHORT>(ConsoleHeight() - reservedRows);
        return availableRows > 0 ? std::min<size_t>(8, static_cast<size_t>(availableRows)) : 0;
    }

    std::string PadLine(const std::string& line, SHORT width) const {
        if (static_cast<SHORT>(line.size()) >= width) return line;
        return line + std::string(static_cast<size_t>(width - line.size()), ' ');
    }

    std::string ClipText(const std::string& text, size_t width) const {
        if (text.size() <= width) return text;
        if (width < 4) return text.substr(0, width);
        return text.substr(0, width - 3) + "...";
    }

    std::string DistroLogo(const std::string& distroName) const {
        std::string name = distroName;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        if (name.find("ubuntu") != std::string::npos) return "◉";
        if (name.find("debian") != std::string::npos) return "◈";
        if (name.find("kali") != std::string::npos) return "◢";
        if (name.find("arch") != std::string::npos) return "△";
        if (name.find("fedora") != std::string::npos) return "●";
        if (name.find("alpine") != std::string::npos) return "▲";
        if (name.find("opensuse") != std::string::npos || name.find("suse") != std::string::npos) return "◆";
        if (name.find("oracle") != std::string::npos) return "◎";
        if (name.find("rocky") != std::string::npos) return "●";
        if (name.find("alma") != std::string::npos) return "●";
        return "○";
    }

    std::string FillLine(const std::string& color, SHORT width, char fill = '=') const {
        return color + std::string(static_cast<size_t>(width), fill) + Color::RESET + "\n";
    }

    InputEvent ReadInput() {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        INPUT_RECORD record = {};
        DWORD eventsRead = 0;

        while (true) {
            DWORD waitResult = WaitForSingleObject(hIn, 250);
            if (waitResult == WAIT_TIMEOUT) continue;
            if (waitResult != WAIT_OBJECT_0) break;
            if (!ReadConsoleInputW(hIn, &record, 1, &eventsRead)) break;

            if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
                InputEvent input;
                input.character = key.uChar.UnicodeChar;

                switch (key.wVirtualKeyCode) {
                case VK_UP: input.key = InputKey::Up; break;
                case VK_DOWN: input.key = InputKey::Down; break;
                case VK_LEFT: input.key = InputKey::Left; break;
                case VK_RIGHT: input.key = InputKey::Right; break;
                case VK_RETURN: input.key = InputKey::Enter; break;
                case VK_SPACE: input.key = InputKey::Space; break;
                case VK_F9: input.key = InputKey::ProcessList; break;
                case VK_F8: input.key = InputKey::Lifecycle; break;
                case VK_F7: input.key = InputKey::FullDiagnostics; break;
                default:
                    if (key.uChar.UnicodeChar != L'\0') {
                        input.key = InputKey::Character;
                    }
                    break;
                }

                if (input.key != InputKey::None) return input;
            } else if (record.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD& mouse = record.Event.MouseEvent;
                if (mouse.dwEventFlags == MOUSE_WHEELED) {
                    InputEvent input;
                    input.mouseWheel = static_cast<SHORT>(HIWORD(mouse.dwButtonState));
                    input.mousePosition = mouse.dwMousePosition;
                    return input;
                }
                const bool leftPressed = (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
                const bool leftClicked = mouse.dwEventFlags == 0 && leftPressed;
                const bool leftDoubleClicked = mouse.dwEventFlags == DOUBLE_CLICK && leftPressed;
                if (leftClicked || leftDoubleClicked) {
                    InputEvent input;
                    input.mouseClick = true;
                    input.doubleClick = leftDoubleClicked;
                    input.mousePosition = mouse.dwMousePosition;
                    return input;
                }
            }
        }

        InputEvent input;
        input.readError = true;
        return input;
    }

    void SelectTab(int tab) {
        if (tab >= 1 && tab <= 7) {
            currentTab = tab;
        }
    }

    void RefreshDiagnosticsProcesses() {
        diagnosticsProcesses.clear();
        selectedProcess = 0;
        diagnosticsProcessOffset = 0;
        if (cachedDistros.empty() || selectedRow >= cachedDistros.size()) return;
        if (cachedDistros[selectedRow].state != "Running") {
            SetStatus("Selected distro is stopped. Start it first to view processes.", Color::YELLOW);
            return;
        }
        diagnosticsProcesses = WSL::ListProcesses(Sys::Utf8ToWide(cachedDistros[selectedRow].name));
    }

    void MoveDiagnosticSelection(int delta) {
        if (!showDiagnosticsProcesses || diagnosticsProcesses.empty()) return;

        if (delta < 0) {
            if (selectedProcess > 0) --selectedProcess;
        } else if (selectedProcess + 1 < diagnosticsProcesses.size()) {
            ++selectedProcess;
        }

        const size_t visibleRows = VisibleDiagnosticProcessRows();
        if (visibleRows == 0) return;
        if (selectedProcess < diagnosticsProcessOffset) {
            diagnosticsProcessOffset = selectedProcess;
        } else if (selectedProcess >= diagnosticsProcessOffset + visibleRows) {
            diagnosticsProcessOffset = selectedProcess - visibleRows + 1;
        }
    }

    void MoveHelpSelection(int delta) {
        if (currentTab != 6) return;
        const size_t visibleRows = VisibleHelpRows();
        if (visibleRows == 0) return;

        if (delta < 0) {
            if (helpScrollOffset > 0) --helpScrollOffset;
        } else {
            const size_t lineCount = 37;
            if (helpScrollOffset + visibleRows < lineCount) ++helpScrollOffset;
        }
    }

    void MoveCatalog(int delta) {
        if (cachedCatalog.empty()) return;
        const size_t visibleRows = VisibleCatalogRows();
        if (visibleRows == 0) return;

        if (delta < 0) {
            if (catalogOffset > 0) --catalogOffset;
        } else if (catalogOffset + visibleRows < cachedCatalog.size()) {
            ++catalogOffset;
        }
    }

    void MoveContainerSelection(int delta) {
        if (cachedContainers.empty()) return;
        const size_t visibleRows = VisibleContainerRows();
        if (visibleRows == 0) return;

        if (delta < 0) {
            if (containerOffset > 0) --containerOffset;
        } else if (containerOffset + visibleRows < cachedContainers.size()) {
            ++containerOffset;
        }
    }

    void MoveFullDiagnostics(int delta) {
        if (!showFullDiagnostics || diagnosticsReportLines.empty()) return;
        const size_t visibleRows = VisibleFullDiagnosticsRows();
        if (visibleRows == 0) return;

        if (delta < 0) {
            if (diagnosticsReportOffset > 0) --diagnosticsReportOffset;
        } else if (diagnosticsReportOffset + visibleRows < diagnosticsReportLines.size()) {
            ++diagnosticsReportOffset;
        }
    }

    void RebuildDiagnosticsReport() {
        diagnosticsReportLines.clear();
        auto append = [&](const std::string& heading, const std::string& value) {
            diagnosticsReportLines.push_back(heading);
            std::istringstream lines(value);
            std::string line;
            while (std::getline(lines, line)) diagnosticsReportLines.push_back(line);
            if (value.empty()) diagnosticsReportLines.push_back("(no output)");
        };
        append("WSL SUBSYSTEM STATUS", diagnosticsStatus);
        append("INSTALLED BINARY REVISIONS", diagnosticsVersion);
        diagnosticsReportOffset = 0;
    }

    void LoadDiagnostics() {
        auto status = Sys::RunWithSpinner("Reading WSL status", [&]() {
            return Sys::Exec({ L"--status" }, 5000);
        });
        diagnosticsStatus = status.stdOut.empty() ? status.stdErr : status.stdOut;

        auto version = Sys::RunWithSpinner("Reading WSL version", [&]() {
            return Sys::Exec({ L"--version" }, 5000);
        });
        diagnosticsVersion = version.stdOut.empty() ? version.stdErr : version.stdOut;
        RebuildDiagnosticsReport();
    }

    void ClearDiagnostics() {
        diagnosticsStatus = "Not loaded. Press [F7] to open Full Diagnostics.";
        diagnosticsVersion.clear();
        diagnosticsReportLines.clear();
        diagnosticsReportOffset = 0;
        diagnosticsProcesses.clear();
        selectedProcess = 0;
        diagnosticsProcessOffset = 0;
        showDiagnosticsProcesses = false;
    }

    void ClearProcessList() {
        diagnosticsProcesses.clear();
        selectedProcess = 0;
        diagnosticsProcessOffset = 0;
        showDiagnosticsProcesses = false;
    }

    void HandleMouse(const InputEvent& input) {
        const SHORT x = input.mousePosition.X;
        const SHORT y = input.mousePosition.Y;

        if (y == 4) {
            for (size_t tab = 0; tab < tabHitBoxes.size(); ++tab) {
                if (x >= tabHitBoxes[tab].first && x < tabHitBoxes[tab].second) {
                    SelectTab(static_cast<int>(tab + 1));
                    break;
                }
            }
            return;
        }

        if (currentTab == 1 && y >= 10 && !cachedDistros.empty()) {
            const size_t row = static_cast<size_t>(y - 10);
            if (row < cachedDistros.size()) {
                selectedRow = row;
                if (input.doubleClick) HandleActionKeys(13);
            }
        }
    }

    void SetStatus(const std::string& msg, const std::string& col = Color::GREEN) {
        statusMsg = msg;
        statusColor = col;
    }

    void CycleTheme() {
        const auto next = static_cast<int>(currentTheme) + 1;
        currentTheme = static_cast<ThemeName>(next % 4);
        ApplyTheme(currentTheme);
        SetStatus("Theme: " + std::string(currentTheme == ThemeName::Silver ? "Silver" : currentTheme == ThemeName::Blue ? "Blue" : currentTheme == ThemeName::Orange ? "Orange" : "Black") + ".", Color::CYAN);
    }

    void CycleGlyphStyle() {
        const auto next = static_cast<int>(currentGlyphStyle) + 1;
        currentGlyphStyle = static_cast<GlyphStyle>(next % 3);
        ApplyGlyphs(currentGlyphStyle);
        SetStatus("Glyphs: " + std::string(currentGlyphStyle == GlyphStyle::Ascii ? "Ascii" : currentGlyphStyle == GlyphStyle::Unicode ? "Unicode" : "Nerd Font") + ".", Color::MAGENTA);
    }

    void RefreshDistroCache() {
        cachedDistros = WSL::ListDistros();
        if (cachedDistros.empty()) {
            selectedRow = 0;
        } else if (selectedRow >= cachedDistros.size()) {
            selectedRow = cachedDistros.size() - 1;
        }
    }

    std::string PromptLine(const std::string& promptText) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(hIn, &mode);

        FlushConsoleInputBuffer(hIn);
        std::cin.clear();

        SetConsoleMode(hIn, mode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
        std::cout << "\033[?25h" << promptText << std::flush;

        std::string input;
        std::getline(std::cin, input);

        SetConsoleMode(hIn, mode);
        FlushConsoleInputBuffer(hIn);
        std::cout << "\033[?25l";

        const auto begin = input.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        const auto end = input.find_last_not_of(" \t\r\n");
        return input.substr(begin, end - begin + 1);
    }

    bool ConfirmAction(const std::string& action, const std::string& details) {
        std::cout << "\033[?25h" << Color::RED << "\n [!] " << action << " " << details << " (y/N): " << Color::RESET;
        std::string confirmation = PromptLine("");
        std::cout << "\033[?25l";
        return confirmation == "y" || confirmation == "Y";
    }

    fs::path Utf8Path(const std::string& text) const {
        return fs::path(Sys::Utf8ToWide(text));
    }

    std::string RenderHeaderString() {
        std::ostringstream out;
        std::ostringstream tabs;
        const SHORT width = ConsoleWidth();
        SHORT visibleTabWidth = 1;
        tabHitBoxes.clear();
        out << "\033[?2026h\033[2J\033[3J\033[H" << Color::BG_BLUE << Color::HEADER_TEXT << Color::BOLD;
        out << FillLine(Color::BG_BLUE + Color::HEADER_TEXT + Color::BOLD, width);
        out << Color::BG_BLUE << Color::HEADER_TEXT << PadLine("  WSL MANAGER v4.5 -- Windows Subsystem for Linux", width) << Color::RESET << "\n";
        out << Color::BG_BLUE << Color::HEADER_TEXT << PadLine("  Copyright (c) 2024 - 2026 Roberto J Dohnert. All rights reserved.", width) << Color::RESET << "\n";
        out << FillLine(Color::BG_BLUE + Color::HEADER_TEXT + Color::BOLD, width) << Color::RESET;

        auto tab = [&](int id, const std::string& label) {
            const SHORT start = tabHitBoxes.empty() ? 1 : tabHitBoxes.back().second;
            if (id == currentTab) {
                tabs << Color::WHITE << Color::BOLD << " [" << id << "] " << label << " " << Color::RESET << Color::BG_DARK;
            } else {
                tabs << Color::WHITE << "  " << id << "  " << label << "  ";
            }
            const SHORT tabWidth = static_cast<SHORT>((id == currentTab ? 6 : 7) + label.size());
            tabHitBoxes.push_back({ start, static_cast<SHORT>(start + tabWidth) });
            visibleTabWidth = static_cast<SHORT>(visibleTabWidth + tabWidth);
        };

        tabs << Color::BG_DARK << " ";
        tab(1, "Distros");
        tab(2, "Online");
        tab(3, "Import");
        tab(4, "Diag");
        tab(5, "Life");
        tab(6, "Help");
        tab(7, "Containers");
        const SHORT quitWidth = 10;
        tabs << " [Q] Quit ";
        visibleTabWidth = static_cast<SHORT>(visibleTabWidth + quitWidth);
        if (visibleTabWidth < width) {
            tabs << std::string(static_cast<size_t>(width - visibleTabWidth), ' ');
        }
        std::string tabLine = tabs.str();
        out << Color::BG_DARK << tabLine << Color::RESET << "\n";
        out << FillLine(Color::BG_BLUE + Color::HEADER_TEXT + Color::BOLD, width);
        return out.str();
    }

    std::string RenderFooterString() {
        std::ostringstream out;
        const SHORT width = ConsoleWidth();
        out << "\n" << FillLine(Color::BG_DARK, width, '-') ;
        out << PadLine(" Status  : " + statusColor + statusMsg + Color::RESET, width) << Color::CLEAR_LINE << "\n";
        out << PadLine(" Controls: [↑/↓] Select | [←/→] Tabs | [Enter/Space] Activate | [1-7] Tabs | [T] Theme | [G] Glyphs | Mouse | [Q] Exit", width) << Color::CLEAR_LINE << "\n";
        return out.str();
    }

public:
    void Run() {
        ApplyTheme(currentTheme);
        ApplyGlyphs(currentGlyphStyle);

        ConsoleGuard guard;
        if (!guard.IsReady()) {
            std::cerr << "WSL Manager requires an interactive Windows console with VT and mouse input support.\n";
            return;
        }
        Sys::RunWithSpinner("Loading installed WSL distributions", [&]() {
            RefreshDistroCache();
            return ProcessResult{ 0, "", "" };
        });

        while (true) {
            std::cout << "\033[?2026h";
            if (currentTab == 1) RenderExplorer();
            else if (currentTab == 2) RenderCatalog();
            else if (currentTab == 3) RenderImportPage();
            else if (currentTab == 4) RenderDiagnostics();
            else if (currentTab == 5) RenderLifecycle();
            else if (currentTab == 6) RenderHelp();
            else if (currentTab == 7) RenderContainers();
            std::cout << "\033[?2026l" << std::flush;

            InputEvent input = ReadInput();
            if (input.readError) {
                std::cerr << "Console input failed: " << GetLastError() << "\n";
                break;
            }
            if (input.mouseWheel != 0) {
                const int steps = input.mouseWheel / WHEEL_DELTA;
                if (currentTab == 4 && showDiagnosticsProcesses) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        MoveDiagnosticSelection(steps > 0 ? -1 : 1);
                    }
                } else if (currentTab == 6) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        MoveHelpSelection(steps > 0 ? -1 : 1);
                    }
                } else if (currentTab == 2) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        MoveCatalog(steps > 0 ? -1 : 1);
                    }
                } else if (currentTab == 7) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        MoveContainerSelection(steps > 0 ? -1 : 1);
                    }
                } else if (currentTab == 4 && showFullDiagnostics) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        MoveFullDiagnostics(steps > 0 ? -1 : 1);
                    }
                } else if (currentTab == 1 && !cachedDistros.empty()) {
                    for (int step = 0; step < std::abs(steps); ++step) {
                        if (steps > 0) {
                            if (selectedRow > 0) --selectedRow;
                        } else if (selectedRow + 1 < cachedDistros.size()) {
                            ++selectedRow;
                        }
                    }
                }
                continue;
            }
            if (input.mouseClick) {
                HandleMouse(input);
                continue;
            }

            if (input.key == InputKey::Up) {
                if (currentTab == 4) {
                    if (showDiagnosticsProcesses) MoveDiagnosticSelection(-1);
                    else if (showFullDiagnostics) MoveFullDiagnostics(-1);
                } else if (currentTab == 6) {
                    MoveHelpSelection(-1);
                } else if (currentTab == 2) {
                    MoveCatalog(-1);
                } else if (currentTab == 7) {
                    MoveContainerSelection(-1);
                } else if (selectedRow > 0) {
                    --selectedRow;
                }
                continue;
            }
            if (input.character == 't' || input.character == 'T') {
                CycleTheme();
                continue;
            }
            if (input.character == 'g' || input.character == 'G') {
                CycleGlyphStyle();
                continue;
            }
            if (input.key == InputKey::Down) {
                if (currentTab == 4) {
                    if (showDiagnosticsProcesses) MoveDiagnosticSelection(1);
                    else if (showFullDiagnostics) MoveFullDiagnostics(1);
                } else if (currentTab == 6) {
                    MoveHelpSelection(1);
                } else if (currentTab == 2) {
                    MoveCatalog(1);
                } else if (currentTab == 7) {
                    MoveContainerSelection(1);
                } else if (!cachedDistros.empty() && selectedRow + 1 < cachedDistros.size()) {
                    ++selectedRow;
                }
                continue;
            }
            if (input.key == InputKey::ProcessList) {
                if (currentTab == 4) HandleTabInput(4, 0, input);
                continue;
            }
            if (input.key == InputKey::FullDiagnostics) {
                if (currentTab == 4) HandleTabInput(4, 0, input);
                continue;
            }
            if (input.key == InputKey::Lifecycle) {
                SelectTab(5);
                continue;
            }
            if (input.key == InputKey::Left) {
                SelectTab(currentTab == 1 ? 7 : currentTab - 1);
                continue;
            }
            if (input.key == InputKey::Right) {
                SelectTab(currentTab == 7 ? 1 : currentTab + 1);
                continue;
            }

            int ch = input.character;
            if (input.key == InputKey::Enter || input.key == InputKey::Space) ch = 13;
            if (ch == 'q' || ch == 'Q') break;
            if (currentTab == 5 && ch >= '1' && ch <= '3') {
                HandleTabInput(5, ch, input);
                continue;
            }
            if (ch >= '1' && ch <= '7') {
                SelectTab(ch - '0');
                continue;
            }

            if (currentTab == 1) HandleActionKeys(ch);
            else HandleTabInput(currentTab, ch, input);
        }
    }

    void HandleTabInput(int tab, int ch, const InputEvent& input) {
        if (tab == 7) {
            if (ch == 'r' || ch == 'R') {
                RefreshContainerCache();
            } else if (ch == 's' || ch == 'S') {
                RunSelectedContainerAction(L"start");
            } else if (ch == 'x' || ch == 'X') {
                RunSelectedContainerAction(L"stop");
            } else if (ch == 'd' || ch == 'D') {
                RunSelectedContainerAction(L"remove");
            } else if (ch == 13 || ch == 'm' || ch == 'M') {
                currentTab = 1;
            }
            return;
        }

        if (tab == 2) {
            if (ch == 'i' || ch == 'I') {
                if (!onlineCatalogLoaded || cachedCatalog.empty()) {
                    SetStatus("Load the online catalog before selecting a distro to install.", Color::YELLOW);
                } else {
                    catalogOffset = std::min(catalogOffset, cachedCatalog.size() - 1);
                    const OnlineDistro& selected = cachedCatalog[catalogOffset];
                    if (!Sys::LaunchInstallWindow(Sys::Utf8ToWide(selected.id))) {
                        SetStatus("Could not start installation for " + selected.id + ".", Color::RED);
                    } else {
                        SetStatus("Installation process spawned for " + selected.id + ".", Color::GREEN);
                    }
                }
            } else if (ch == 'r' || ch == 'R') {
                cachedCatalog = WSL::ListOnline();
                onlineCatalogLoaded = true;
                catalogOffset = 0;
                SetStatus("Online catalog refreshed.", Color::GREEN);
            } else if (ch == 'm' || ch == 'M' || ch == 13) {
                currentTab = 1;
            }
            return;
        }

        if (tab == 3) {
            if (ch == 13 || ch == 'i' || ch == 'I') {
                RunImportWorkflow();
            } else if (ch == 'm' || ch == 'M') {
                currentTab = 1;
            }
            return;
        }

        if (tab == 5) {
            if (ch == '1') {
                if (!ConfirmAction("Shutdown all running WSL distributions", "This stops every active distro and frees host memory.")) {
                    SetStatus("Shutdown cancelled.", Color::YELLOW);
                    return;
                }
                Sys::RunWithSpinner("Shutting down WSL subsystem", [&]() {
                    return Sys::Exec({ L"--shutdown" }, 30000);
                });
                RefreshDistroCache();
                SetStatus("All WSL distributions stopped. Memory cleared.", Color::GREEN);
            } else if (ch == '2') {
                if (!ConfirmAction("Update WSL components", "This downloads and installs the latest Microsoft WSL components.")) {
                    SetStatus("Update cancelled.", Color::YELLOW);
                    return;
                }
                lifecycleDetails = "Preparing WSL component update...\n";
                auto res = Sys::RunWithSpinner("Updating WSL components", [&]() {
                    return Sys::Exec({ L"--update" }, 300000);
                });
                lifecycleDetails += "Command: wsl.exe --update\n";
                lifecycleDetails += "Exit code: " + std::to_string(res.exitCode) + "\n";
                lifecycleDetails += res.stdOut.empty() ? "Output: (none)\n" : "Output:\n" + res.stdOut + "\n";
                if (!res.stdErr.empty()) lifecycleDetails += "Diagnostics:\n" + res.stdErr + "\n";
                lifecycleDetails += res.success() ? "Result: WSL update completed successfully." : "Result: WSL update failed.";
                SetStatus(res.success() ? "WSL updated successfully." : res.stdErr, res.success() ? Color::GREEN : Color::RED);
            } else if (ch == '3') {
                if (!ConfirmAction("Rollback the WSL kernel", "This reverts to the previous WSL kernel version.")) {
                    SetStatus("Kernel rollback cancelled.", Color::YELLOW);
                    return;
                }
                auto res = Sys::RunWithSpinner("Rolling back WSL kernel", [&]() {
                    return Sys::Exec({ L"--update", L"--rollback" }, 60000);
                });
                SetStatus(res.success() ? "WSL kernel rolled back." : res.stdErr, res.success() ? Color::GREEN : Color::RED);
            }
            if (ch != '1' && ch != '2' && ch != '3' && input.key != InputKey::Left && input.key != InputKey::Right) {
                currentTab = 1;
            }
            return;
        }

        if (tab == 4) {
            if (input.key == InputKey::FullDiagnostics) {
                if (showFullDiagnostics) {
                    showFullDiagnostics = false;
                    ClearDiagnostics();
                    SetStatus("Full Diagnostics closed. WSL details cleared.", Color::CYAN);
                } else {
                    ClearProcessList();
                    LoadDiagnostics();
                    showFullDiagnostics = true;
                    SetStatus("Full Diagnostics shown. F7 hides it.", Color::CYAN);
                }
            } else if (input.key == InputKey::ProcessList) {
                if (showDiagnosticsProcesses) {
                    ClearProcessList();
                    SetStatus("Process list closed.", Color::CYAN);
                } else {
                    showFullDiagnostics = false;
                    RefreshDiagnosticsProcesses();
                    showDiagnosticsProcesses = true;
                    SetStatus("Process list shown. F9 hides it.", Color::CYAN);
                }
            } else if (ch == 'r' || ch == 'R' || ch == 13) {
                if (!showFullDiagnostics) {
                    SetStatus("Press F7 to open Full Diagnostics first.", Color::YELLOW);
                } else {
                    LoadDiagnostics();
                    if (showDiagnosticsProcesses) RefreshDiagnosticsProcesses();
                    SetStatus("Diagnostics refreshed.", Color::GREEN);
                }
            } else if (ch == 'k' || ch == 'K' || ch == 'x' || ch == 'X') {
                if (!showDiagnosticsProcesses) {
                    SetStatus("Press F9 to show the process list first.", Color::YELLOW);
                } else if (cachedDistros.empty() || selectedRow >= cachedDistros.size() || diagnosticsProcesses.empty() || selectedProcess >= diagnosticsProcesses.size()) {
                    SetStatus("No diagnostic process is selected.", Color::YELLOW);
                } else {
                    const WslProcess process = diagnosticsProcesses[selectedProcess];
                    auto result = Sys::RunWithSpinner("Stopping PID " + std::to_string(process.pid), [&]() {
                        return WSL::StopProcess(Sys::Utf8ToWide(cachedDistros[selectedRow].name), process.pid);
                    });
                    RefreshDiagnosticsProcesses();
                    SetStatus(result.success() ? "PID " + std::to_string(process.pid) + " stopped and diagnostics refreshed." : "Unable to stop PID " + std::to_string(process.pid) + ".", result.success() ? Color::GREEN : Color::RED);
                }
            } else {
                currentTab = 1;
            }
            return;
        }

        if (tab == 6) {
            currentTab = 1;
        }
    }

    void RefreshContainerCache() {
        ProcessResult result = Sys::RunWithSpinner("Loading wslc containers", [&]() {
            return WSL::ListContainers(cachedContainers);
        });
        containersLoaded = true;
        containerOffset = 0;
        if (!result.success()) {
            cachedContainers.clear();
            SetStatus(result.stdErr.empty() ? "Could not load wslc containers." : result.stdErr, Color::RED);
        } else if (cachedContainers.empty()) {
            SetStatus("No containers found. Press [R] to refresh or use wslc run to create one.", Color::YELLOW);
        } else {
            SetStatus("Loaded " + std::to_string(cachedContainers.size()) + " containers.", Color::GREEN);
        }
    }

    void RunSelectedContainerAction(const std::wstring& action) {
        if (!containersLoaded || cachedContainers.empty() || containerOffset >= cachedContainers.size()) {
            SetStatus("No container is selected. Press [R] to refresh the list.", Color::YELLOW);
            return;
        }

        const std::string containerId = cachedContainers[containerOffset].id;
        if (action == L"remove" && !ConfirmAction(
            "Remove container " + containerId,
            "This deletes the container and its writable layer; attached volumes are retained.")) {
            SetStatus("Container removal cancelled.", Color::YELLOW);
            return;
        }

        const std::string actionLabel = action == L"start" ? "Starting " : action == L"stop" ? "Stopping " : "Removing ";
        ProcessResult result = Sys::RunWithSpinner(actionLabel + containerId, [&]() {
            return WSL::RunContainerAction(action, containerId);
        });
        if (!result.success()) {
            SetStatus(result.stdErr.empty() ? "wslc container action failed." : result.stdErr, Color::RED);
            return;
        }

        RefreshContainerCache();
        if (result.stdOut.empty()) {
            SetStatus(actionLabel + containerId + " completed.", Color::GREEN);
        } else {
            SetStatus(ClipText(result.stdOut, 120), Color::GREEN);
        }
    }

private:
    void RenderExplorer() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] INSTALLED DISTRIBUTIONS\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";
        out << "  DEF   LOGO NAME                    STATE            WSL VERSION   MANAGEMENT\n";
        out << "--------------------------------------------------------------------------------------------------------\n";

        if (cachedDistros.empty()) {
            out << Color::YELLOW << "  No distributions installed. Press [2] to install from the online catalog.\n" << Color::RESET;
        } else {
            for (size_t i = 0; i < cachedDistros.size(); ++i) {
                const auto& d = cachedDistros[i];
                bool isSel = (i == selectedRow);

                std::string prefix = isSel ? Color::BG_HIGHLIGHT + Color::WHITE + " >" : "  ";
                std::string defMark = d.isDefault ? Color::GREEN + "[*]" + Color::RESET : "   ";
                std::string stateCol = (d.state == "Running") ? Color::GREEN : Color::WHITE;

                out << prefix << " " << defMark << "  "
                    << DistroLogo(d.name) << " " << std::left << std::setw(25) << d.name
                    << stateCol << std::setw(17) << d.state << Color::RESET
                    << std::setw(14) << d.version
                    << (isSel ? Color::CYAN + "[Enter]Launch [T]Stop [E]Export [D]Del" : "")
                    << Color::RESET << Color::CLEAR_LINE << "\n";
            }
        }

        out << "--------------------------------------------------------------------------------------------------------\n";
        out << Color::BOLD << "SELECTED ACTIONS:\n" << Color::RESET;
        out << " [Enter] Launch Window   [T] Terminate   [S] Set Default   [2] Upgrade to WSL2   [E] Export   [D] Unregister\n";
        out << RenderFooterString();
        std::cout << out.str() << std::flush;
    }

    void HandleActionKeys(int ch) {
        if (cachedDistros.empty() || selectedRow >= cachedDistros.size()) {
            if (ch == 'r' || ch == 'R') {
                RefreshDistroCache();
                SetStatus("Distro list refreshed.");
            }
            return;
        }

        const auto& cur = cachedDistros[selectedRow];
        std::wstring wName = Sys::Utf8ToWide(cur.name);

        if (currentTab == 1) {
            if (ch == 13) { // ENTER: Launch
                const std::string distroName = cur.name;
                if (!Sys::IsValidDistroName(wName)) {
                    SetStatus("Refusing to launch invalid distro name.", Color::RED);
                    return;
                }
                const bool launched = Sys::LaunchInTerminal(wName);
                if (launched) {
                    cachedDistros[selectedRow].state = "Running";
                    SetStatus("Entered " + distroName + ".", Color::GREEN);
                } else {
                    SetStatus("Could not launch " + distroName + ".", Color::RED);
                }
            } else if (ch == 't' || ch == 'T') { // Terminate
                auto res = Sys::RunWithSpinner("Stopping " + cur.name, [&]() {
                    return Sys::Exec({ L"-t", wName }, 20000);
                });
                RefreshDistroCache();
                SetStatus(res.success() ? cur.name + " stopped." : "Error: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
            } else if (ch == 's' || ch == 'S') { // Set Default
                auto res = Sys::RunWithSpinner("Setting " + cur.name + " as default", [&]() {
                    return Sys::Exec({ L"--set-default", wName }, 15000);
                });
                RefreshDistroCache();
                SetStatus(res.success() ? cur.name + " is now the default distro." : "Error: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
            } else if (ch == '2') { // Convert to WSL2
                auto res = Sys::RunWithSpinner("Converting " + cur.name + " to WSL 2 architecture", [&]() {
                    return Sys::Exec({ L"--set-version", wName, L"2" }, 600000);
                });
                RefreshDistroCache();
                SetStatus(res.success() ? cur.name + " converted to WSL 2." : "Conversion error: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
            } else if (ch == 'e' || ch == 'E') { // Export
                std::string tar = PromptLine(" Enter destination tarball path (e.g. D:\\backup.tar): ");
                if (!tar.empty()) {
                    std::wstring wTar = Sys::Utf8ToWide(tar);
                    if (!Sys::IsValidFilesystemPath(wTar)) {
                        SetStatus("Refusing to export to an invalid path.", Color::RED);
                        return;
                    }
                    fs::path outFile = fs::path(wTar);
                    if (fs::exists(outFile) && fs::is_directory(outFile)) {
                        SetStatus("Export path must be a file, not a directory.", Color::RED);
                        return;
                    }
                    if (fs::exists(outFile)) {
                        std::cout << "\033[?25h" << Color::RED << "\n [!] Overwrite existing file '" << tar << "'? (y/N): " << Color::RESET;
                        std::string overwrite = PromptLine("");
                        std::cout << "\033[?25l";
                        if (!(overwrite == "y" || overwrite == "Y")) {
                            SetStatus("Export cancelled to avoid overwriting an existing file.", Color::YELLOW);
                            return;
                        }
                    }
                    auto res = Sys::RunWithSpinner("Exporting " + cur.name + " to " + tar, [&]() {
                        return Sys::Exec({ L"--export", wName, wTar }, 600000);
                    });
                    SetStatus(res.success() ? "Export successful: " + tar : "Export error: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
                }
            } else if (ch == 'd' || ch == 'D') { // Unregister
                std::cout << "\033[?25h" << Color::RED << "\n [!] Permanently DELETE & UNREGISTER '" << cur.name << "'? (y/N): " << Color::RESET;
                std::string confirmation = PromptLine("");
                int confirm = confirmation.empty() ? 0 : confirmation[0];
                std::cout << "\033[?25l";
                if (confirm == 'y' || confirm == 'Y') {
                    if (!Sys::IsValidDistroName(wName)) {
                        SetStatus("Refusing to unregister invalid distro name.", Color::RED);
                        return;
                    }
                    auto res = Sys::RunWithSpinner("Unregistering " + cur.name, [&]() {
                        return Sys::Exec({ L"--unregister", wName }, 120000);
                    });
                    RefreshDistroCache();
                    SetStatus(res.success() ? "Unregistered " + cur.name : "Unregister error: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
                }
            } else if (ch == 'r' || ch == 'R') {
                RefreshDistroCache();
                SetStatus("Distro list refreshed.");
            }
        }
    }

    void RenderCatalog() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] AVAILABLE ONLINE DISTRIBUTIONS (wsl --list --online)\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";
        out << "  LOGO IDENTIFIER (NAME)        FRIENDLY NAME\n";
        out << "--------------------------------------------------------------------------------------------------------\n";

        if (!onlineCatalogLoaded) {
            out << Color::YELLOW << "Press [R] to load the online distribution catalog.\n" << Color::RESET;
        } else if (cachedCatalog.empty()) {
            out << Color::YELLOW << "No online distributions were returned. Press [R] to retry.\n" << Color::RESET;
        } else {
            const size_t visibleRows = VisibleCatalogRows();
            catalogOffset = std::min(catalogOffset, cachedCatalog.size() - 1);
            const size_t end = visibleRows == 0 ? catalogOffset : std::min(cachedCatalog.size(), catalogOffset + visibleRows);
            out << "  Showing " << (catalogOffset + 1) << "-" << end << " of " << cachedCatalog.size() << " distributions\n";
            for (size_t index = catalogOffset; index < end; ++index) {
                const auto& item = cachedCatalog[index];
                const bool selected = index == catalogOffset;
                out << (selected ? Color::BG_HIGHLIGHT + Color::WHITE + "> " : "  ")
                    << DistroLogo(item.id) << " " << Color::GREEN << std::left << std::setw(27) << item.id
                    << Color::RESET << item.friendlyName << Color::CLEAR_LINE << "\n";
            }
            out << "  [Up/Down] or mouse wheel scroll\n";
        }

        out << "--------------------------------------------------------------------------------------------------------\n";
        out << " [I] Install Selected Distro Identifier    [M] Main Menu\n";
        out << RenderFooterString();
        std::cout << out.str() << std::flush;

    }

    void RenderContainers() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] WSL CONTAINERS (wslc)\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";
        out << "  SELECTED CONTAINER ID\n";
        out << "--------------------------------------------------------------------------------------------------------\n";

        if (!containersLoaded) {
            out << Color::YELLOW << "Press [R] to load containers managed by wslc.\n" << Color::RESET;
        } else if (cachedContainers.empty()) {
            out << Color::YELLOW << "No containers found. Create one with wslc run, then press [R] to refresh.\n" << Color::RESET;
        } else {
            const size_t visibleRows = VisibleContainerRows();
            containerOffset = std::min(containerOffset, cachedContainers.size() - 1);
            const size_t end = std::min(cachedContainers.size(), containerOffset + visibleRows);
            out << "  Showing " << (containerOffset + 1) << "-" << end << " of " << cachedContainers.size() << " containers\n";
            for (size_t index = containerOffset; index < end; ++index) {
                const bool selected = index == containerOffset;
                out << (selected ? Color::BG_HIGHLIGHT + Color::WHITE + "> " : "  ")
                    << Color::GREEN << cachedContainers[index].id << Color::RESET << Color::CLEAR_LINE << "\n";
            }
            out << "  [Up/Down] or mouse wheel select a container\n";
        }

        out << "--------------------------------------------------------------------------------------------------------\n";
        out << " [R] Refresh   [S] Start   [X] Stop   [D] Remove   [M] Main Menu\n";
        out << RenderFooterString();
        std::cout << out.str() << std::flush;
    }

    void RenderImportPage() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] IMPORT CUSTOM TARBALL OR CONTAINER ROOTFS\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n\n";
        out << "Press [Enter] or [I] to begin the import wizard.\n";
        out << "Press [M] to return to Distro Explorer.\n";
        out << RenderFooterString();
        std::cout << out.str() << std::flush;
    }

    void RunImportWorkflow() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] IMPORT CUSTOM TARBALL OR CONTAINER ROOTFS\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n\n";
        std::cout << out.str() << std::flush;

        std::string name = PromptLine(" 1. Target Distro Instance Name : ");
        if (name.empty()) { currentTab = 1; return; }
        std::wstring wName = Sys::Utf8ToWide(name);
        if (!Sys::IsValidDistroName(wName)) {
            SetStatus("Import Error: Invalid distro name.", Color::RED);
            currentTab = 1;
            return;
        }

        std::string dest = PromptLine(" 2. Target Installation Folder  : ");
        if (dest.empty()) { currentTab = 1; return; }
        std::wstring wDest = Sys::Utf8ToWide(dest);
        if (!Sys::IsValidFilesystemPath(wDest)) {
            SetStatus("Import Error: Invalid destination folder path.", Color::RED);
            currentTab = 1;
            return;
        }

        std::string tar = PromptLine(" 3. Path to Rootfs Tarball File : ");
        std::error_code fileError;
        fs::path tarPath = Utf8Path(tar);
        if (tar.empty() || !Sys::IsValidFilesystemPath(Sys::Utf8ToWide(tar), true) || !fs::exists(tarPath, fileError) || fileError) {
            SetStatus("Import Error: Source file does not exist.", Color::RED);
            currentTab = 1;
            return;
        }

        fs::path destinationPath = Utf8Path(dest);
        fileError.clear();
        if (fs::exists(destinationPath) && !fs::is_directory(destinationPath)) {
            SetStatus("Import Error: Destination exists but is not a directory.", Color::RED);
            currentTab = 1;
            return;
        }
        fs::create_directories(destinationPath, fileError);
        if (fileError) {
            SetStatus("Import Error: Cannot create destination folder: " + fileError.message(), Color::RED);
            currentTab = 1;
            return;
        }

        std::wstring wTar = Sys::Utf8ToWide(tar);

        auto res = Sys::RunWithSpinner("Importing archive (WSL 2)", [&]() {
            return Sys::Exec({ L"--import", wName, wDest, wTar, L"--version", L"2" }, 600000);
        });

        RefreshDistroCache();
        SetStatus(res.success() ? "Import completed for " + name : "Import failed: " + res.stdErr, res.success() ? Color::GREEN : Color::RED);
        currentTab = 1;
    }

    void RenderDiagnostics() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] SUBSYSTEM DIAGNOSTICS & ENGINE HEALTH\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";

        if (showDiagnosticsProcesses) {
            const SHORT width = ConsoleWidth();
            const size_t innerWidth = width > 6 ? static_cast<size_t>(width - 6) : 1;
            const size_t visibleRows = VisibleDiagnosticProcessRows();
            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";
            out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << "PROCESSES (F9 closes)" << " |" << Color::RESET << "\n";
            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";
            if (cachedDistros.empty() || selectedRow >= cachedDistros.size()) {
                out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << "No distro is selected." << " |" << Color::RESET << "\n";
            } else {
                const std::string title = "Processes in " + cachedDistros[selectedRow].name;
                out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << ClipText(title, innerWidth) << " |" << Color::RESET << "\n";
                if (diagnosticsProcesses.empty()) {
                    out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << "No processes returned. Refresh with R." << " |" << Color::RESET << "\n";
                } else if (visibleRows == 0) {
                    out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << "Enlarge the console to view processes." << " |" << Color::RESET << "\n";
                } else {
                    const size_t maxOffset = diagnosticsProcesses.size() - 1;
                    diagnosticsProcessOffset = std::min(diagnosticsProcessOffset, maxOffset);
                    const size_t end = std::min(diagnosticsProcesses.size(), diagnosticsProcessOffset + visibleRows);
                    for (size_t index = diagnosticsProcessOffset; index < end; ++index) {
                        const auto& process = diagnosticsProcesses[index];
                        const bool selected = index == selectedProcess;
                        const size_t commandWidth = innerWidth > 36 ? innerWidth - 36 : 8;
                        std::ostringstream processLine;
                        processLine << (selected ? "> " : "  ") << std::left << std::setw(9) << process.pid
                            << std::setw(21) << ClipText(process.name, 20) << ClipText(process.command, commandWidth);
                        out << Color::BG_DARK << "| " << std::setw(static_cast<int>(innerWidth)) << std::left << ClipText(processLine.str(), innerWidth) << " |" << Color::RESET << "\n";
                    }
                }
            }
            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";
            out << "[Up/Down] or mouse wheel scroll   [K/X] Stop selected   [F9] Close\n";
            out << RenderFooterString();
            std::cout << out.str() << std::flush;
            return;
        }

        if (showFullDiagnostics) {
            const SHORT width = ConsoleWidth();
            const size_t innerWidth = width > 6 ? static_cast<size_t>(width - 6) : 1;
            const size_t visibleRows = VisibleFullDiagnosticsRows();
            if (diagnosticsReportLines.empty()) RebuildDiagnosticsReport();
            if (!diagnosticsReportLines.empty()) {
                diagnosticsReportOffset = std::min(diagnosticsReportOffset, diagnosticsReportLines.size() - 1);
            }

            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";
            out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth)) << "FULL DIAGNOSTICS (F7 closes)" << " |" << Color::RESET << "\n";
            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";

            const size_t end = visibleRows == 0 ? diagnosticsReportOffset : std::min(diagnosticsReportLines.size(), diagnosticsReportOffset + visibleRows);
            for (size_t index = diagnosticsReportOffset; index < end; ++index) {
                out << Color::BG_DARK << "| " << std::left << std::setw(static_cast<int>(innerWidth))
                    << ClipText(diagnosticsReportLines[index], innerWidth) << " |" << Color::RESET << "\n";
            }
            for (size_t row = end - diagnosticsReportOffset; row < visibleRows; ++row) {
                out << Color::BG_DARK << "| " << std::setw(static_cast<int>(innerWidth)) << " " << " |" << Color::RESET << "\n";
            }
            out << Color::BG_DARK << "+-" << std::string(innerWidth, '-') << "-+" << Color::RESET << "\n";
            out << "Showing " << (diagnosticsReportLines.empty() ? 0 : diagnosticsReportOffset + 1) << "-" << end << " of " << diagnosticsReportLines.size() << " report lines\n";
            out << "[Up/Down] or mouse wheel scroll   [F7] Close full Diagnostics   [R/Enter] Refresh\n";
            out << RenderFooterString();
            std::cout << out.str() << std::flush;
            return;
        }
        
        out << " [F7] Open Full Diagnostics   [F9] Open Processes\n";
        out << Color::CYAN << "Press [F7] for WSL details or [F9] for the selected distro's processes.\n" << Color::RESET;
        
        out << RenderFooterString();
        std::cout << out.str() << std::flush;
    }

    void RenderLifecycle() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] SUBSYSTEM LIFECYCLE & KERNEL MANAGEMENT\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";
        out << " [1] Clean Subsystem Shutdown (wsl --shutdown)\n";
        out << "     Terminates all running VM instances and frees locked RAM back to host OS.\n\n";
        out << " [2] Check and Update Kernel (wsl --update)\n";
        out << "     Pulls the newest Microsoft WSL2 Linux kernel package.\n\n";
        out << " [3] Rollback Kernel (wsl --update --rollback)\n";
        out << "     Reverts back to the previous stable kernel revision.\n\n";
        out << " [F8] Open Lifecycle   [1] Shutdown   [2] Update   [3] Rollback\n";
        out << "\n" << Color::BOLD << "Last lifecycle operation:\n" << Color::RESET;
        std::istringstream detailLines(lifecycleDetails);
        std::string detailLine;
        while (std::getline(detailLines, detailLine)) {
            out << "  " << ClipText(detailLine, ConsoleWidth() > 4 ? static_cast<size_t>(ConsoleWidth() - 4) : 1) << "\n";
        }
        out << " [M] Return to Distro Explorer\n";
        out << RenderFooterString();
        std::cout << out.str() << std::flush;

    }

    void RenderHelp() {
        std::ostringstream out;
        out << RenderHeaderString();
        out << Color::BOLD << Color::CYAN << "[*] REFERENCE MANUAL & TROUBLESHOOTING\n" << Color::RESET;
        out << "--------------------------------------------------------------------------------------------------------\n";
        const std::vector<std::string> helpLines = {
            "1. Keyboard Navigation:",
            "   - [UP/DOWN]    Select a distro or scroll this Help page.",
            "   - [LEFT/RIGHT] Move between management tabs.",
            "   - [ENTER/SPACE] Activate the selected action.",
            "   - [MOUSE WHEEL] Scroll distro, process, or Help lists.",
            "   - [1-6]        Jump immediately to any management tab.",
            "   - [T]          Cycle color theme (Silver, Blue, Orange, Black).",
            "   - [G]          Cycle glyph style (Ascii, Unicode, Nerd Font).",
            "2. Explorer Examples:",
            "   - Select Ubuntu with [Up/Down], then press [Enter] to open it.",
            "   - Press [T] to stop, [S] to set default, or [R] to refresh.",
            "   - Press [E], then enter D:\\Backups\\ubuntu.tar to export.",
            "   - Press [D], then type Y and Enter to unregister permanently.",
            "3. Diagnostics Examples:",
            "   - Select a distro, open Diagnostics with [4], then press [F7].",
            "   - Press [F9], use [Up/Down] or the wheel to select a process.",
            "   - Press [K] or [X] to send the selected process SIGTERM.",
            "   - Press [F9] again to hide the process list.",
            "4. Import and Online Store:",
            "   - Online Store: press [2], then [R] to load distro identifiers.",
            "   - Import: press [3], then [Enter], and provide the requested paths.",
            "   - Example rootfs: C:\\WSL\\Images\\debian-rootfs.tar.",
            "5. Useful WSL Commands:",
            "   - List distros: wsl --list --verbose",
            "   - Run a command: wsl -d Ubuntu -- uname -a",
            "   - Stop all distros: wsl --shutdown",
            "   - Manage containers: open [7] Containers; create with wslc run.",
            "6. Lifecycle and Shortcuts:",
            "   - [F7] opens a framed Full Diagnostics window; press F7 to close it.",
            "   - [F8] opens Lifecycle; [1] shutdown, [2] update, [3] rollback.",
            "   - Lifecycle Update displays command output and exit details.",
            "7. Virtualization Errors:",
            "   - 0x80370102 usually means virtualization is disabled in firmware.",
            "   - 0x80040326 usually means the WSL2 kernel needs an update.",
            "8. Container Rootfs Workflow:",
            "   - Export: docker export <container_id> > rootfs.tar",
            "   - Import: register the rootfs from Tab [3].",
            "9. Page Controls:",
            "   - Help shows 10 lines at a time. Use Up/Down or the mouse wheel.",
            "   - [Q] exits from any screen."
        };
        const size_t visibleRows = VisibleHelpRows();
        const size_t maxOffset = helpLines.empty() ? 0 : helpLines.size() - 1;
        helpScrollOffset = std::min(helpScrollOffset, maxOffset);
        const size_t end = visibleRows == 0 ? helpScrollOffset : std::min(helpLines.size(), helpScrollOffset + visibleRows);
        out << "Showing " << (helpLines.empty() ? 0 : helpScrollOffset + 1) << "-" << end << " of " << helpLines.size() << " help lines\n";
        for (size_t index = helpScrollOffset; index < end; ++index) {
            out << ClipText(helpLines[index], ConsoleWidth() > 2 ? static_cast<size_t>(ConsoleWidth() - 2) : 1) << "\n";
        }
        out << "\n[Up/Down] or mouse wheel scroll   [1-6] Tabs   [Q] Exit";
        std::cout << out.str() << std::flush;
    }
};

int RunWslManager() {
    App app;
    app.Run();
    return 0;
}
