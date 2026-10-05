/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// ============================================================================
//               tmux Single File Index (tmux.cpp)
// ============================================================================
//
// FILE INDEX:
// ----------------------------------------------------------------------------
// [01] Win32 Headers, OS Integration & Clipboard API
// [02] Unicode 15.0 / Nerd Font Character Width Engine (wcwidth)
// [03] 24-bit TrueColor & 256-Color Cell Model (with Wide-Cell Support)
// [04] Full VT100/xterm State Machine (DA1/DA2/DSR Queries, DECSTBM, SGR Colon)
// [05] Win32 Input Record -> xterm VT Sequence Encoder (F1-F12, Modifiers)
// [06] ConPTY Process Subsystem & Bidirectional Pipe Management
// [07] BSP Split Layout Engine & Interactive Border Drag Resizing
// [08] Multi-Window Workspace Tabs & Scrollback Buffer
// [09] Event-Driven Double-Buffered Compositor & In-App Mouse Router
// [10] CLI Parser, Interactive Help Overlay & Entry Point
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <cstring>

// --- Terminal Control Constants ---
#define VT_ESC "\x1b"
#define VT_ALT_BUF_ON "\x1b[?1049h"
#define VT_ALT_BUF_OFF "\x1b[?1049l"
#define VT_HIDE_CURSOR "\x1b[?25l"
#define VT_SHOW_CURSOR "\x1b[?25h"
#define VT_MOUSE_ON "\x1b[?1000h\x1b[?1002h\x1b[?1006h"
#define VT_MOUSE_OFF "\x1b[?1000l\x1b[?1002l\x1b[?1006l"
#define VT_RESET "\x1b[0m"

#pragma comment(lib, "user32.lib")

// Helpers for wide/narrow string conversion
inline std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

inline std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring DetectDefaultShell() {
    return L"powershell.exe";
}

std::string ShellTabName(const std::wstring& shellCmd) {
    std::string shell = WStringToString(shellCmd);
    size_t firstArg = shell.find(' ');
    std::string exe = firstArg == std::string::npos ? shell : shell.substr(0, firstArg);
    exe.erase(std::remove(exe.begin(), exe.end(), '"'), exe.end());
    size_t slash = exe.find_last_of("\\/");
    if (slash != std::string::npos) exe = exe.substr(slash + 1);
    size_t dot = exe.find_last_of('.');
    if (dot != std::string::npos) exe = exe.substr(0, dot);
    return exe.empty() ? "shell" : exe;
}

// ============================================================================
// [01] Win32 Headers, OS Integration & Clipboard API
// ============================================================================
bool SetOSClipboard(const std::wstring& text) {
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    size_t sz = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.data(), sz);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    return true;
}

std::wstring GetOSClipboard() {
    if (!OpenClipboard(NULL)) return L"";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    std::wstring str;
    if (hData) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(hData));
        if (p) { str = p; GlobalUnlock(hData); }
    }
    CloseClipboard();
    return str;
}

struct Rect {
    int x = 0, y = 0, width = 0, height = 0;
    bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

// ============================================================================
// [02] Unicode 15.0 / Nerd Font Character Width Engine (wcwidth)
// ============================================================================
int GetCodePointWidth(uint32_t cp) {
    if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
    // Zero-width combining characters
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
        (cp >= 0xFE20 && cp <= 0xFE2F)) return 0;

    // East Asian Wide, Emojis, Powerline & Nerd Font PUA
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2329 && cp <= 0x232A) ||
        (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0xFE30 && cp <= 0xFE6F) ||
        (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1F300 && cp <= 0x1F64F) || (cp >= 0x1F680 && cp <= 0x1F6FF) ||
        (cp >= 0x2600 && cp <= 0x26FF)   || (cp >= 0x2700 && cp <= 0x27BF)   ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x1FA70 && cp <= 0x1FAFF)) {
        return 2;
    }
    return 1;
}

// ============================================================================
// [03] 24-bit TrueColor & 256-Color Cell Model
// ============================================================================
struct Color {
    uint8_t r = 204, g = 204, b = 204;
    bool isDefault = true;
    bool isIndexed = false;
    uint8_t index = 7;

    static Color DefaultFg() { Color c; c.r = 204; c.g = 204; c.b = 204; c.isDefault = true; return c; }
    static Color DefaultBg() { Color c; c.r = 12;  c.g = 12;  c.b = 12;  c.isDefault = true; return c; }
    static Color Rgb(uint8_t r, uint8_t g, uint8_t b) { Color c; c.r = r; c.g = g; c.b = b; c.isDefault = false; return c; }
    static Color Idx(uint8_t i) { Color c; c.index = i; c.isIndexed = true; c.isDefault = false; return c; }

    bool operator==(const Color& o) const {
        if (isDefault && o.isDefault) return true;
        if (isDefault != o.isDefault) return false;
        if (isIndexed && o.isIndexed) return index == o.index;
        return r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

struct Cell {
    wchar_t ch = L' ';
    Color fg = Color::DefaultFg();
    Color bg = Color::DefaultBg();
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool inverse = false;
    uint8_t width = 1; // 1 = standard, 2 = wide leader, 0 = wide trailer

    Cell() = default;
    Cell(wchar_t c, Color f = Color::DefaultFg(), Color b = Color::DefaultBg(),
         bool bl = false, bool dm = false, bool it = false, bool ul = false, bool inv = false, uint8_t w = 1)
        : ch(c), fg(f), bg(b), bold(bl), dim(dm), italic(it), underline(ul), inverse(inv), width(w) {}

    bool operator==(const Cell& o) const {
        return ch == o.ch && fg == o.fg && bg == o.bg &&
               bold == o.bold && dim == o.dim && italic == o.italic &&
               underline == o.underline && inverse == o.inverse && width == o.width;
    }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

struct UiTheme {
    const char* name;
    Color statusBg;
    Color statusPrefixBg;
    Color statusFg;
    Color border;
    Color activeBorder;
    Color promptBg;
    Color helpBg;
    Color helpFg;
};

const std::vector<UiTheme>& AvailableThemes() {
    static const std::vector<UiTheme> themes = {
        { "blue",   Color::Rgb(28, 84, 190),  Color::Rgb(245, 185, 40), Color::Rgb(255, 255, 255), Color::Rgb(80, 120, 190),  Color::Rgb(100, 190, 255), Color::Rgb(190, 45, 45),  Color::Rgb(20, 50, 140),   Color::Rgb(255, 255, 255) },
        { "black",  Color::Rgb(30, 30, 30),   Color::Rgb(210, 210, 210), Color::Rgb(245, 245, 245), Color::Rgb(85, 85, 85),    Color::Rgb(230, 230, 230), Color::Rgb(120, 30, 30),  Color::Rgb(18, 18, 18),    Color::Rgb(245, 245, 245) },
        { "orange", Color::Rgb(220, 105, 22), Color::Rgb(70, 45, 25),   Color::Rgb(20, 20, 20),    Color::Rgb(170, 95, 45),   Color::Rgb(255, 190, 90),  Color::Rgb(110, 45, 20),  Color::Rgb(150, 65, 20),   Color::Rgb(255, 250, 235) },
        { "silver", Color::Rgb(188, 194, 204), Color::Rgb(70, 80, 92),   Color::Rgb(15, 20, 28),    Color::Rgb(135, 145, 158), Color::Rgb(250, 250, 255), Color::Rgb(95, 65, 95),   Color::Rgb(78, 86, 96),    Color::Rgb(245, 248, 252) },
        { "pink",   Color::Rgb(225, 88, 156), Color::Rgb(70, 30, 55),   Color::Rgb(30, 15, 25),    Color::Rgb(190, 85, 135),  Color::Rgb(255, 185, 220), Color::Rgb(125, 35, 85),  Color::Rgb(145, 45, 100),  Color::Rgb(255, 245, 250) },
        { "yellow", Color::Rgb(236, 205, 55), Color::Rgb(60, 55, 20),   Color::Rgb(20, 20, 10),    Color::Rgb(175, 155, 55),  Color::Rgb(255, 245, 135), Color::Rgb(120, 95, 15),  Color::Rgb(135, 115, 30),  Color::Rgb(255, 250, 215) }
    };
    return themes;
}

std::string TrimCopy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end - 1])) --end;
    return s.substr(start, end - start);
}

std::string LowerCopy(std::string s) {
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

std::string UpperCopy(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

int ThemeIndexByName(const std::string& name) {
    std::string needle = LowerCopy(TrimCopy(name));
    const auto& themes = AvailableThemes();
    for (size_t i = 0; i < themes.size(); ++i) {
        if (needle == themes[i].name) return (int)i;
    }
    return -1;
}

struct KeyCombo {
    bool ctrl = true;
    bool alt = true;
    bool shift = false;
    WORD vk = 'T';

    bool Matches(const KEY_EVENT_RECORD& ker) const {
        bool hasCtrl = (ker.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool hasAlt = (ker.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        bool hasShift = (ker.dwControlKeyState & SHIFT_PRESSED) != 0;
        return ker.wVirtualKeyCode == vk && hasCtrl == ctrl && hasAlt == alt && hasShift == shift;
    }

    std::string Display() const {
        std::string out;
        if (ctrl) out += "Ctrl+";
        if (alt) out += "Alt+";
        if (shift) out += "Shift+";
        if (vk >= VK_F1 && vk <= VK_F12) out += "F" + std::to_string(vk - VK_F1 + 1);
        else if (vk >= 'A' && vk <= 'Z') out += (char)vk;
        else out += "VK" + std::to_string(vk);
        return out;
    }
};

struct HelperKeys {
    KeyCombo prefixKey{ true, false, false, 'B' };
    KeyCombo splitVertical{ false, true, false, 'V' };
    KeyCombo splitHorizontal{ false, true, false, 'H' };
    KeyCombo newWindow{ false, true, false, 'T' };
    KeyCombo nextWindow{ false, true, false, 'N' };
    KeyCombo previousWindow{ false, true, false, 'P' };
    KeyCombo zoomPane{ false, true, false, 'Z' };
    KeyCombo killPane{ false, true, false, 'W' };
    KeyCombo killPaneAlternate{ false, true, false, 'Q' };
    KeyCombo exitSession{ true, true, false, 'Q' };
    KeyCombo themeCycle{ true, true, false, 'T' };
};

KeyCombo* FindHelperKey(HelperKeys& keys, const std::string& action) {
    std::string name = LowerCopy(TrimCopy(action));
    if (name == "prefix" || name == "prefix-key") return &keys.prefixKey;
    if (name == "split-vertical" || name == "splitv") return &keys.splitVertical;
    if (name == "split-horizontal" || name == "splith") return &keys.splitHorizontal;
    if (name == "new-window" || name == "new-tab") return &keys.newWindow;
    if (name == "next-window" || name == "next-tab") return &keys.nextWindow;
    if (name == "previous-window" || name == "prev-window" || name == "previous-tab" || name == "prev-tab") return &keys.previousWindow;
    if (name == "zoom" || name == "zoom-pane") return &keys.zoomPane;
    if (name == "kill-pane" || name == "kill") return &keys.killPane;
    if (name == "kill-pane-alt" || name == "kill-alternate" || name == "kill-secondary") return &keys.killPaneAlternate;
    if (name == "exit" || name == "detach" || name == "exit-session") return &keys.exitSession;
    if (name == "theme" || name == "theme-cycle" || name == "cycle-theme") return &keys.themeCycle;
    return nullptr;
}

bool ParseKeyCombo(const std::string& value, KeyCombo& combo) {
    KeyCombo parsed;
    parsed.ctrl = false;
    parsed.alt = false;
    parsed.shift = false;
    parsed.vk = 0;

    std::stringstream ss(value);
    std::string part;
    while (std::getline(ss, part, '+')) {
        std::string token = LowerCopy(TrimCopy(part));
        if (token.empty()) continue;
        if (token == "ctrl" || token == "control") parsed.ctrl = true;
        else if (token == "alt") parsed.alt = true;
        else if (token == "shift") parsed.shift = true;
        else if (token.size() == 1 && std::isalnum((unsigned char)token[0])) parsed.vk = (WORD)std::toupper((unsigned char)token[0]);
        else if (token == "left") parsed.vk = VK_LEFT;
        else if (token == "right") parsed.vk = VK_RIGHT;
        else if (token == "up") parsed.vk = VK_UP;
        else if (token == "down") parsed.vk = VK_DOWN;
        else if (token == "pgup" || token == "pageup") parsed.vk = VK_PRIOR;
        else if (token == "pgdn" || token == "pagedown") parsed.vk = VK_NEXT;
        else if (token.size() >= 2 && token[0] == 'f') {
            int fn = std::atoi(token.c_str() + 1);
            if (fn >= 1 && fn <= 12) parsed.vk = (WORD)(VK_F1 + fn - 1);
            else return false;
        } else {
            return false;
        }
    }

    if (parsed.vk == 0 || (!parsed.ctrl && !parsed.alt && !parsed.shift)) return false;
    combo = parsed;
    return true;
}

// ============================================================================
// [04] Full VT100/xterm State Machine (Queries, DECSTBM, SGR Colons)
// ============================================================================
class VirtualScreen {
public:
    int width = 80, height = 24;
    int cursorX = 0, cursorY = 0;
    int savedX = 0, savedY = 0;
    bool cursorVisible = true;

    Color curFg = Color::DefaultFg();
    Color curBg = Color::DefaultBg();
    bool curBold = false, curDim = false, curItalic = false, curUnderline = false, curInverse = false;

    std::vector<std::vector<Cell>> primaryGrid;
    std::vector<std::vector<Cell>> altGrid;
    bool usingAltBuffer = false;

    // Mouse Tracking Modes for Child TUIs
    bool mouseTracking = false;
    bool mouseSgrMode = false;
    bool mouseButtonMotion = false;
    bool mouseAllMotion = false;

    int scrollTop = 0, scrollBottom = 23;
    std::deque<std::vector<Cell>> scrollback;
    size_t maxScrollback = 3000;
    int scrollOffset = 0;

    enum State { Ground, Escape, Csi, Osc, Utf8_2, Utf8_3, Utf8_4 } parseState = Ground;
    std::vector<int> csiParams;
    int currentParam = 0;
    bool hasParam = false;
    bool csiPrivate = false;
    std::string oscString;
    uint32_t utf8Code = 0;
    int utf8BytesLeft = 0;

    std::function<void(const std::string&)> sendResponseCallback;
    std::mutex mtx;

    VirtualScreen(int w, int h) : width(std::max(1, w)), height(std::max(1, h)) {
        scrollTop = 0;
        scrollBottom = height - 1;
        ResizeGrid(width, height);
    }

    std::vector<std::vector<Cell>>& ActiveGrid() {
        return usingAltBuffer ? altGrid : primaryGrid;
    }

    void ResizeGrid(int w, int h) {
        if (w <= 0 || h <= 0) return;
        width = w;
        height = h;
        scrollTop = 0;
        scrollBottom = height - 1;

        primaryGrid.resize(height);
        for (auto& r : primaryGrid) r.resize(width, Cell{ L' ', curFg, curBg });

        altGrid.resize(height);
        for (auto& r : altGrid) r.resize(width, Cell{ L' ', curFg, curBg });

        cursorX = std::clamp(cursorX, 0, width - 1);
        cursorY = std::clamp(cursorY, 0, height - 1);
    }

    void ScrollRegionUp(int top, int bottom) {
        auto& g = ActiveGrid();
        if (top < 0 || bottom >= height || top >= bottom) return;
        if (!usingAltBuffer && top == 0) {
            scrollback.push_back(g[top]);
            if (scrollback.size() > maxScrollback) scrollback.pop_front();
        }
        for (int y = top; y < bottom; ++y) g[y] = g[y + 1];
        g[bottom] = std::vector<Cell>(width, Cell{ L' ', curFg, curBg });
    }

    void ScrollRegionDown(int top, int bottom) {
        auto& g = ActiveGrid();
        if (top < 0 || bottom >= height || top >= bottom) return;
        for (int y = bottom; y > top; --y) g[y] = g[y - 1];
        g[top] = std::vector<Cell>(width, Cell{ L' ', curFg, curBg });
    }

    void PutCodePoint(uint32_t cp) {
        if (cp == L'\r') { cursorX = 0; return; }
        if (cp == L'\n') {
            if (cursorY == scrollBottom) ScrollRegionUp(scrollTop, scrollBottom);
            else if (cursorY < height - 1) cursorY++;
            return;
        }
        if (cp == L'\b') { if (cursorX > 0) cursorX--; return; }
        if (cp == L'\t') {
            cursorX = (cursorX + 8) & ~7;
            if (cursorX >= width) {
                cursorX = 0;
                if (cursorY == scrollBottom) ScrollRegionUp(scrollTop, scrollBottom);
                else if (cursorY < height - 1) cursorY++;
            }
            return;
        }
        if (cp < 32 && cp != L'\t') return;

        int w = GetCodePointWidth(cp);
        if (w == 0) return; // Zero-width combining

        if (cursorX + w > width) {
            cursorX = 0;
            if (cursorY == scrollBottom) ScrollRegionUp(scrollTop, scrollBottom);
            else if (cursorY < height - 1) cursorY++;
        }

        Cell c;
        c.ch = (wchar_t)cp; c.fg = curFg; c.bg = curBg;
        c.bold = curBold; c.dim = curDim; c.italic = curItalic;
        c.underline = curUnderline; c.inverse = curInverse;
        c.width = (uint8_t)w;

        ActiveGrid()[cursorY][cursorX] = c;
        if (w == 2 && cursorX + 1 < width) {
            Cell trailer = c;
            trailer.ch = L' ';
            trailer.width = 0; // Trailing placeholder
            ActiveGrid()[cursorY][cursorX + 1] = trailer;
        }
        cursorX += w;
    }

    void ParseSgr() {
        if (csiParams.empty()) {
            curFg = Color::DefaultFg(); curBg = Color::DefaultBg();
            curBold = curDim = curItalic = curUnderline = curInverse = false;
            return;
        }
        for (size_t i = 0; i < csiParams.size(); ++i) {
            int p = csiParams[i];
            if (p == 0) {
                curFg = Color::DefaultFg(); curBg = Color::DefaultBg();
                curBold = curDim = curItalic = curUnderline = curInverse = false;
            } else if (p == 1) curBold = true;
            else if (p == 2) curDim = true;
            else if (p == 3) curItalic = true;
            else if (p == 4) curUnderline = true;
            else if (p == 7) curInverse = true;
            else if (p == 22) { curBold = false; curDim = false; }
            else if (p == 23) curItalic = false;
            else if (p == 24) curUnderline = false;
            else if (p == 27) curInverse = false;
            else if (p >= 30 && p <= 37) curFg = Color::Idx(p - 30);
            else if (p == 39) curFg = Color::DefaultFg();
            else if (p >= 40 && p <= 47) curBg = Color::Idx(p - 40);
            else if (p == 49) curBg = Color::DefaultBg();
            else if (p >= 90 && p <= 97) curFg = Color::Idx(p - 90 + 8);
            else if (p >= 100 && p <= 107) curBg = Color::Idx(p - 100 + 8);
            else if (p == 38 || p == 48) {
                bool isFg = (p == 38);
                if (i + 1 < csiParams.size()) {
                    if (csiParams[i + 1] == 5 && i + 2 < csiParams.size()) { // 256 Colors
                        Color col = Color::Idx((uint8_t)csiParams[i + 2]);
                        if (isFg) curFg = col; else curBg = col;
                        i += 2;
                    } else if (csiParams[i + 1] == 2) { // TrueColor
                        // Handles 38;2;r;g;b or colon sub-params with empty alpha 38:2::r:g:b
                        int rIdx = (int)i + 2;
                        if (rIdx < (int)csiParams.size() && csiParams[rIdx] == 0 && (rIdx + 3 < (int)csiParams.size())) rIdx++;
                        if (rIdx + 2 < (int)csiParams.size()) {
                            Color col = Color::Rgb((uint8_t)csiParams[rIdx], (uint8_t)csiParams[rIdx + 1], (uint8_t)csiParams[rIdx + 2]);
                            if (isFg) curFg = col; else curBg = col;
                            i = rIdx + 2;
                        }
                    }
                }
            }
        }
    }

    void ExecuteCsi(char finalChar) {
        if (hasParam) csiParams.push_back(currentParam);
        int p1 = csiParams.size() > 0 ? csiParams[0] : 0;
        int p2 = csiParams.size() > 1 ? csiParams[1] : 0;
        auto& g = ActiveGrid();

        if (csiPrivate) {
            if (finalChar == 'h') {
                if (p1 == 25) cursorVisible = true;
                else if (p1 == 1049 || p1 == 47) usingAltBuffer = true;
                else if (p1 == 1000) { mouseTracking = true; }
                else if (p1 == 1002) { mouseTracking = true; mouseButtonMotion = true; }
                else if (p1 == 1003) { mouseTracking = true; mouseAllMotion = true; }
                else if (p1 == 1006) { mouseSgrMode = true; }
            } else if (finalChar == 'l') {
                if (p1 == 25) cursorVisible = false;
                else if (p1 == 1049 || p1 == 47) usingAltBuffer = false;
                else if (p1 == 1000 || p1 == 1002 || p1 == 1003) { mouseTracking = false; mouseButtonMotion = false; mouseAllMotion = false; }
                else if (p1 == 1006) { mouseSgrMode = false; }
            }
            return;
        }

        switch (finalChar) {
        case 'c': // DA1 / DA2 Terminal Device Queries
            if (sendResponseCallback) {
                if (csiParams.empty() || p1 == 0) {
                    sendResponseCallback("\x1b[?62;1;2;6;7;8;9c"); // VT220 response
                }
            }
            break;
        case 'n': // DSR Queries
            if (sendResponseCallback) {
                if (p1 == 6) { // Cursor Position Request
                    std::string resp = "\x1b[" + std::to_string(cursorY + 1) + ";" + std::to_string(cursorX + 1) + "R";
                    sendResponseCallback(resp);
                } else if (p1 == 5) {
                    sendResponseCallback("\x1b[0n"); // Status OK
                }
            }
            break;
        case 'H': case 'f':
            cursorY = std::clamp((p1 > 0 ? p1 - 1 : 0), 0, height - 1);
            cursorX = std::clamp((p2 > 0 ? p2 - 1 : 0), 0, width - 1);
            break;
        case 'A': cursorY = std::max(0, cursorY - (p1 > 0 ? p1 : 1)); break;
        case 'B': cursorY = std::min(height - 1, cursorY + (p1 > 0 ? p1 : 1)); break;
        case 'C': cursorX = std::min(width - 1, cursorX + (p1 > 0 ? p1 : 1)); break;
        case 'D': cursorX = std::max(0, cursorX - (p1 > 0 ? p1 : 1)); break;
        case 'G': cursorX = std::clamp((p1 > 0 ? p1 - 1 : 0), 0, width - 1); break;
        case 'd': cursorY = std::clamp((p1 > 0 ? p1 - 1 : 0), 0, height - 1); break;
        case 'J':
            if (p1 == 0) {
                for (int x = cursorX; x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
                for (int y = cursorY + 1; y < height; ++y)
                    for (int x = 0; x < width; ++x) g[y][x] = Cell{ L' ', curFg, curBg };
            } else if (p1 == 1) {
                for (int y = 0; y < cursorY; ++y)
                    for (int x = 0; x < width; ++x) g[y][x] = Cell{ L' ', curFg, curBg };
                for (int x = 0; x <= cursorX && x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            } else if (p1 == 2 || p1 == 3) {
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) g[y][x] = Cell{ L' ', curFg, curBg };
            }
            break;
        case 'K':
            if (p1 == 0) {
                for (int x = cursorX; x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            } else if (p1 == 1) {
                for (int x = 0; x <= cursorX && x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            } else if (p1 == 2) {
                for (int x = 0; x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            }
            break;
        case 'L': { // Insert Line (Vim/Micro)
            int lines = p1 > 0 ? p1 : 1;
            for (int n = 0; n < lines; ++n) ScrollRegionDown(cursorY, scrollBottom);
            break;
        }
        case 'M': { // Delete Line (Vim/Micro)
            int lines = p1 > 0 ? p1 : 1;
            for (int n = 0; n < lines; ++n) ScrollRegionUp(cursorY, scrollBottom);
            break;
        }
        case 'P': { // Delete Char
            int chars = p1 > 0 ? p1 : 1;
            for (int x = cursorX; x < width - chars; ++x) g[cursorY][x] = g[cursorY][x + chars];
            for (int x = width - chars; x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            break;
        }
        case '@': { // Insert Char
            int chars = p1 > 0 ? p1 : 1;
            for (int x = width - 1; x >= cursorX + chars; --x) g[cursorY][x] = g[cursorY][x - chars];
            for (int x = cursorX; x < cursorX + chars && x < width; ++x) g[cursorY][x] = Cell{ L' ', curFg, curBg };
            break;
        }
        case 'r': // DECSTBM Set Scrolling Margins
            scrollTop = std::clamp((p1 > 0 ? p1 - 1 : 0), 0, height - 1);
            scrollBottom = std::clamp((p2 > 0 ? p2 - 1 : height - 1), 0, height - 1);
            if (scrollTop >= scrollBottom) { scrollTop = 0; scrollBottom = height - 1; }
            cursorX = 0; cursorY = 0;
            break;
        case 'm': ParseSgr(); break;
        case 's': savedX = cursorX; savedY = cursorY; break;
        case 'u': cursorX = savedX; cursorY = savedY; break;
        }
    }

    void ProcessBytes(const char* data, size_t len) {
        std::lock_guard<std::mutex> lock(mtx);
        for (size_t i = 0; i < len; ++i) {
            uint8_t byte = (uint8_t)data[i];
            if (parseState == Ground) {
                if (byte < 0x80) {
                    if (byte == 0x1b) { parseState = Escape; continue; }
                    PutCodePoint(byte);
                } else if ((byte & 0xE0) == 0xC0) { utf8Code = byte & 0x1F; utf8BytesLeft = 1; parseState = Utf8_2; }
                else if ((byte & 0xF0) == 0xE0) { utf8Code = byte & 0x0F; utf8BytesLeft = 2; parseState = Utf8_3; }
                else if ((byte & 0xF8) == 0xF0) { utf8Code = byte & 0x07; utf8BytesLeft = 3; parseState = Utf8_4; }
                continue;
            }
            if (parseState >= Utf8_2 && parseState <= Utf8_4) {
                if ((byte & 0xC0) == 0x80) {
                    utf8Code = (utf8Code << 6) | (byte & 0x3F);
                    if (--utf8BytesLeft == 0) { PutCodePoint(utf8Code); parseState = Ground; }
                } else { parseState = Ground; }
                continue;
            }
            if (parseState == Escape) {
                if (byte == '[') {
                    parseState = Csi;
                    csiParams.clear();
                    currentParam = 0;
                    hasParam = false;
                    csiPrivate = false;
                } else if (byte == ']') { parseState = Osc; oscString.clear(); }
                else if (byte == '7') { savedX = cursorX; savedY = cursorY; parseState = Ground; }
                else if (byte == '8') { cursorX = savedX; cursorY = savedY; parseState = Ground; }
                else if (byte == '>') { // Secondary DA prefix
                    if (sendResponseCallback) sendResponseCallback("\x1b[>0;10;0c");
                    parseState = Ground;
                } else { parseState = Ground; }
                continue;
            }
            if (parseState == Csi) {
                if (byte >= '0' && byte <= '9') { currentParam = currentParam * 10 + (byte - '0'); hasParam = true; }
                else if (byte == ';' || byte == ':') { csiParams.push_back(currentParam); currentParam = 0; hasParam = false; }
                else if (byte == '?') { csiPrivate = true; }
                else if (byte >= 0x40 && byte <= 0x7E) { ExecuteCsi((char)byte); parseState = Ground; }
                continue;
            }
            if (parseState == Osc) {
                if (byte == 0x07 || byte == 0x1b) parseState = Ground;
                else oscString += (char)byte;
                continue;
            }
        }
    }
};

// ============================================================================
// [05] Win32 Input Record -> xterm VT Sequence Encoder
// ============================================================================
std::string EncodeKeyEvent(const KEY_EVENT_RECORD& ker) {
    if (!ker.bKeyDown) return "";

    char ch = ker.uChar.AsciiChar;
    WORD vk = ker.wVirtualKeyCode;
    DWORD ctrl = ker.dwControlKeyState;
    bool isShift = (ctrl & SHIFT_PRESSED) != 0;
    bool isCtrl  = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    bool isAlt   = (ctrl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;

    if (ch != 0 && !isAlt && vk != VK_TAB && vk != VK_RETURN && vk != VK_BACK && vk != VK_ESCAPE) {
        return std::string(1, ch);
    }

    if (vk == VK_RETURN) return "\r";
    if (vk == VK_BACK)   return "\x7f";
    if (vk == VK_ESCAPE) return "\x1b";
    if (vk == VK_TAB) {
        if (isShift) return "\x1b[Z";
        return "\t";
    }

    if (vk >= VK_F1 && vk <= VK_F12) {
        int f = vk - VK_F1 + 1;
        if (f <= 4) {
            const char* fmap[] = { "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS" };
            return fmap[f - 1];
        } else {
            const int codeMap[] = { 15, 17, 18, 19, 20, 21, 23, 24 };
            return "\x1b[" + std::to_string(codeMap[f - 5]) + "~";
        }
    }

    int mod = 1;
    if (isShift) mod += 1;
    if (isAlt)   mod += 2;
    if (isCtrl)  mod += 4;

    auto formatCsi = [mod](char finalCh, const std::string& baseTilde = "") -> std::string {
        if (mod > 1) {
            if (!baseTilde.empty()) return "\x1b[" + baseTilde + ";" + std::to_string(mod) + "~";
            return "\x1b[1;" + std::to_string(mod) + finalCh;
        }
        if (!baseTilde.empty()) return "\x1b[" + baseTilde + "~";
        return std::string("\x1b[") + finalCh;
    };

    switch (vk) {
    case VK_UP:    return formatCsi('A');
    case VK_DOWN:  return formatCsi('B');
    case VK_RIGHT: return formatCsi('C');
    case VK_LEFT:  return formatCsi('D');
    case VK_HOME:  return formatCsi('H', "1");
    case VK_INSERT:return formatCsi(' ', "2");
    case VK_DELETE:return formatCsi(' ', "3");
    case VK_END:   return formatCsi('F', "4");
    case VK_PRIOR: return formatCsi(' ', "5");
    case VK_NEXT:  return formatCsi(' ', "6");
    }

    if (isAlt && ch != 0) return std::string("\x1b") + ch;
    return "";
}

// ============================================================================
// [06] ConPTY Process Subsystem & Async IO
// ============================================================================
class Pane {
public:
    int id;
    std::string title;
    Rect bounds{};
    HPCON hPC{ INVALID_HANDLE_VALUE };
    HANDLE hPipeIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipeOut{ INVALID_HANDLE_VALUE };
    HANDLE hProcess{ INVALID_HANDLE_VALUE };
    std::atomic<bool> alive{ true };
    std::thread readerThread;
    VirtualScreen screen;
    std::function<void()> onDirty;

    Pane(int id, const std::string& title, std::function<void()> dirtyCb)
        : id(id), title(title), screen(80, 24), onDirty(dirtyCb) {
        // Direct feedback response pipeline for DA1/DA2/DSR queries
        screen.sendResponseCallback = [this](const std::string& resp) {
            SendInput(resp);
        };
    }

    ~Pane() {
        alive = false;
        if (hProcess != INVALID_HANDLE_VALUE) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            hProcess = INVALID_HANDLE_VALUE;
        }
        if (hPC != INVALID_HANDLE_VALUE) {
            ClosePseudoConsole(hPC);
            hPC = INVALID_HANDLE_VALUE;
        }
        if (hPipeIn != INVALID_HANDLE_VALUE) {
            CancelIoEx(hPipeIn, NULL);
            CloseHandle(hPipeIn);
            hPipeIn = INVALID_HANDLE_VALUE;
        }
        if (hPipeOut != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipeOut);
            hPipeOut = INVALID_HANDLE_VALUE;
        }
        if (readerThread.joinable()) {
            readerThread.join();
        }
    }

    bool Start(const std::wstring& cmd, int w, int h) {
        bounds.width = w;
        bounds.height = h;
        screen.ResizeGrid(std::max(1, w), std::max(1, h));

        HANDLE hPipePTYIn, hPipePTYOut;
        if (!CreatePipe(&hPipePTYIn, &hPipeOut, NULL, 0)) return false;
        if (!CreatePipe(&hPipeIn, &hPipePTYOut, NULL, 0)) return false;

        COORD size = { (SHORT)screen.width, (SHORT)screen.height };
        HRESULT hr = CreatePseudoConsole(size, hPipePTYIn, hPipePTYOut, 0, &hPC);
        CloseHandle(hPipePTYIn);
        CloseHandle(hPipePTYOut);
        if (FAILED(hr)) return false;

        STARTUPINFOEXW siEx = { 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
        siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);
        InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &bytes);
        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL);

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back(0);

        BOOL success = CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siEx.StartupInfo, &pi);
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        free(siEx.lpAttributeList);
        if (!success) return false;

        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        readerThread = std::thread([this]() {
            char buffer[8192];
            DWORD read;
            while (alive && ReadFile(hPipeIn, buffer, sizeof(buffer), &read, NULL) && read > 0) {
                screen.ProcessBytes(buffer, read);
                if (onDirty) onDirty();
            }
            alive = false;
            if (onDirty) onDirty();
        });

        return true;
    }

    void Resize(int w, int h) {
        bounds.width = w;
        bounds.height = h;
        screen.ResizeGrid(std::max(1, w), std::max(1, h));
        if (hPC != INVALID_HANDLE_VALUE) {
            COORD size = { (SHORT)screen.width, (SHORT)screen.height };
            ResizePseudoConsole(hPC, size);
        }
    }

    void SendInput(const std::string& data) {
        if (hPipeOut != INVALID_HANDLE_VALUE && !data.empty()) {
            DWORD written;
            WriteFile(hPipeOut, data.data(), (DWORD)data.size(), &written, NULL);
        }
    }
};

// ============================================================================
// [07] BSP Split Layout Engine & Border Dragging
// ============================================================================
enum class SplitType { None, Horizontal, Vertical };

struct LayoutNode : public std::enable_shared_from_this<LayoutNode> {
    SplitType split = SplitType::None;
    float ratio = 0.5f;
    std::shared_ptr<LayoutNode> first;
    std::shared_ptr<LayoutNode> second;
    std::shared_ptr<Pane> pane;
    std::weak_ptr<LayoutNode> parent;
    Rect borderRect{};
    int x = 0, y = 0, w = 0, h = 0;

    void CalculateBounds(int nx, int ny, int nw, int nh) {
        x = nx; y = ny; w = nw; h = nh;
        if (split == SplitType::None) {
            if (pane) {
                pane->bounds = { x, y, w, h };
                pane->Resize(w, h);
            }
            return;
        }

        if (split == SplitType::Vertical) { // Left | Right
            int firstW = (int)std::round((w - 1) * ratio);
            firstW = std::max(2, std::min(w - 3, firstW));
            int secondW = w - firstW - 1;
            borderRect = { x + firstW, y, 1, h };
            if (first) first->CalculateBounds(x, y, firstW, h);
            if (second) second->CalculateBounds(x + firstW + 1, y, secondW, h);
        } else if (split == SplitType::Horizontal) { // Top / Bottom
            int firstH = (int)std::round((h - 1) * ratio);
            firstH = std::max(2, std::min(h - 3, firstH));
            int secondH = h - firstH - 1;
            borderRect = { x, y + firstH, w, 1 };
            if (first) first->CalculateBounds(x, y, w, firstH);
            if (second) second->CalculateBounds(x, y + firstH + 1, w, secondH);
        }
    }

    void CollectPanes(std::vector<std::shared_ptr<Pane>>& list) {
        if (split == SplitType::None && pane) list.push_back(pane);
        else {
            if (first) first->CollectPanes(list);
            if (second) second->CollectPanes(list);
        }
    }

    std::shared_ptr<LayoutNode> FindNodeForPane(std::shared_ptr<Pane> p) {
        if (split == SplitType::None && pane == p) return shared_from_this();
        if (first) { auto n = first->FindNodeForPane(p); if (n) return n; }
        if (second) { auto n = second->FindNodeForPane(p); if (n) return n; }
        return nullptr;
    }

    std::shared_ptr<LayoutNode> HitTestBorder(int px, int py) {
        if (split != SplitType::None) {
            if (borderRect.Contains(px, py)) return shared_from_this();
            if (first) { auto n = first->HitTestBorder(px, py); if (n) return n; }
            if (second) { auto n = second->HitTestBorder(px, py); if (n) return n; }
        }
        return nullptr;
    }
};

// ============================================================================
// [08] Multi-Window Workspace Tabs
// ============================================================================
struct Window {
    int id;
    std::string name;
    std::shared_ptr<LayoutNode> rootLayout;
    std::shared_ptr<Pane> activePane;
    bool isZoomed = false;

    Window(int id, const std::string& name) : id(id), name(name) {
        rootLayout = std::make_shared<LayoutNode>();
    }

    std::vector<std::shared_ptr<Pane>> GetPanes() {
        std::vector<std::shared_ptr<Pane>> list;
        rootLayout->CollectPanes(list);
        return list;
    }
};

// ============================================================================
// [09] Event-Driven Double-Buffered Compositor & In-App Mouse Router
// ============================================================================
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

    WmuxEngine() {
        hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    void MarkDirty() {
        isDirty = true;
        cvDirty.notify_one();
    }

    const UiTheme& Theme() const {
        const auto& themes = AvailableThemes();
        return themes[activeThemeIdx % themes.size()];
    }

    bool SetTheme(const std::string& name) {
        int idx = ThemeIndexByName(name);
        if (idx < 0) return false;
        activeThemeIdx = (size_t)idx;
        MarkDirty();
        return true;
    }

    void CycleTheme() {
        const auto& themes = AvailableThemes();
        activeThemeIdx = (activeThemeIdx + 1) % themes.size();
        MarkDirty();
    }

    bool InitConsole() {
        GetConsoleMode(hStdIn, &origInMode);
        GetConsoleMode(hStdOut, &origOutMode);

        DWORD outMode = origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        DWORD inMode = origInMode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
        inMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

        SetConsoleMode(hStdOut, outMode);
        SetConsoleMode(hStdIn, inMode);

        std::cout << VT_ALT_BUF_ON << VT_HIDE_CURSOR << VT_MOUSE_ON << std::flush;
        UpdateSize();
        CreateWindowTab(ShellTabName(defaultShell), defaultShell);
        return true;
    }

    void RestoreConsole() {
        std::cout << VT_MOUSE_OFF << VT_SHOW_CURSOR << VT_ALT_BUF_OFF << std::flush;
        SetConsoleMode(hStdIn, origInMode);
        SetConsoleMode(hStdOut, origOutMode);
    }

    void UpdateSize() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
            int newW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int newH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            if (newW != termW || newH != termH) {
                termW = newW;
                termH = newH;
                // Clear the console to avoid layout remnants
                DWORD written;
                WriteConsoleW(hStdOut, L"\x1b[2J", 5, &written, NULL);
            }
        }
    }

    void CreateWindowTab(const std::string& name, const std::wstring& shellCmd) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto win = std::make_shared<Window>(nextWindowId++, name);
        auto p = std::make_shared<Pane>(nextPaneId++, WStringToString(shellCmd), [this]() { MarkDirty(); });
        win->rootLayout->pane = p;
        win->activePane = p;
        windows.push_back(win);
        activeWindowIdx = windows.size() - 1;

        RecalculateLayout();
        p->Start(shellCmd, p->bounds.width, p->bounds.height);
        MarkDirty();
    }

    void RecalculateLayout() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        int availH = termH - 1;

        if (win->isZoomed && win->activePane) {
            win->activePane->bounds = { 0, 0, termW, availH };
            win->activePane->Resize(termW, availH);
        } else {
            win->rootLayout->CalculateBounds(0, 0, termW, availH);
        }
        MarkDirty();
    }

    void SplitActivePane(SplitType type) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto& win = windows[activeWindowIdx];
        if (win->isZoomed) win->isZoomed = false;

        auto node = win->rootLayout->FindNodeForPane(win->activePane);
        if (!node) return;

        auto existingPane = node->pane;
        auto newPane = std::make_shared<Pane>(nextPaneId++, WStringToString(defaultShell), [this]() { MarkDirty(); });

        node->split = type;
        node->ratio = 0.5f;
        node->pane = nullptr;

        node->first = std::make_shared<LayoutNode>();
        node->first->pane = existingPane;
        node->first->parent = node;

        node->second = std::make_shared<LayoutNode>();
        node->second->pane = newPane;
        node->second->parent = node;

        win->activePane = newPane;
        RecalculateLayout();
        newPane->Start(defaultShell, newPane->bounds.width, newPane->bounds.height);
        MarkDirty();
    }

    void KillPane(std::shared_ptr<Pane> p) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        for (size_t wIdx = 0; wIdx < windows.size(); ++wIdx) {
            auto win = windows[wIdx];
            auto panes = win->GetPanes();
            auto it = std::find(panes.begin(), panes.end(), p);
            if (it != panes.end()) {
                if (panes.size() <= 1) {
                    windows.erase(windows.begin() + wIdx);
                    if (windows.empty()) { isRunning = false; return; }
                    activeWindowIdx = std::min(activeWindowIdx, windows.size() - 1);
                    RecalculateLayout();
                    return;
                }

                auto node = win->rootLayout->FindNodeForPane(p);
                if (!node) return;

                auto parent = node->parent.lock();
                if (!parent) return;

                auto sibling = (parent->first == node) ? parent->second : parent->first;
                parent->split = sibling->split;
                parent->ratio = sibling->ratio;
                parent->pane = sibling->pane;
                parent->first = sibling->first;
                parent->second = sibling->second;
                if (parent->first) parent->first->parent = parent;
                if (parent->second) parent->second->parent = parent;

                auto remaining = win->GetPanes();
                if (win->activePane == p) {
                    win->activePane = remaining.front();
                }
                RecalculateLayout();
                MarkDirty();
                return;
            }
        }
    }

    void KillActivePane() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        if (win->activePane) {
            KillPane(win->activePane);
        }
    }

    // Arms a y/n confirmation instead of killing the active pane immediately.
    void RequestKillActivePaneConfirm() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        if (!win->activePane) return;
        pendingKillPaneTarget = win->activePane;
        pendingKillPaneConfirm = true;
        MarkDirty();
    }

    void ResolveKillPaneConfirm(bool confirmed) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        pendingKillPaneConfirm = false;
        auto target = pendingKillPaneTarget;
        pendingKillPaneTarget.reset();
        if (confirmed && target) KillPane(target);
        MarkDirty();
    }

    void ReapDeadPanes() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        std::vector<std::shared_ptr<Pane>> deadPanes;
        for (auto& win : windows) {
            for (auto& p : win->GetPanes()) {
                if (!p->alive) {
                    deadPanes.push_back(p);
                }
            }
        }
        for (auto& p : deadPanes) {
            KillPane(p);
        }
    }

    bool IsBorderChar(wchar_t ch) {
        return ch == L'-' || ch == L'|' || ch == L'+';
    }

    void SmoothBorders(std::vector<std::vector<Cell>>& frame) {
        int sbY = termH - 1; // Status bar row
        for (int y = 0; y < sbY; ++y) {
            for (int x = 0; x < termW; ++x) {
                wchar_t ch = frame[y][x].ch;
                if (!IsBorderChar(ch)) {
                    continue;
                }

                bool u = (y > 0) && IsBorderChar(frame[y - 1][x].ch);
                bool d = (y < sbY - 1) && IsBorderChar(frame[y + 1][x].ch);
                bool l = (x > 0) && IsBorderChar(frame[y][x - 1].ch);
                bool r = (x < termW - 1) && IsBorderChar(frame[y][x + 1].ch);

                wchar_t newCh = ch;
                if (u && d && l && r) newCh = L'+';
                else if (d && l && r) newCh = L'+';
                else if (u && l && r) newCh = L'+';
                else if (u && d && r) newCh = L'+';
                else if (u && d && l) newCh = L'+';
                else if (d && r) newCh = L'+';
                else if (d && l) newCh = L'+';
                else if (u && r) newCh = L'+';
                else if (u && l) newCh = L'+';
                else if (u || d) newCh = L'|';
                else if (l || r) newCh = L'-';

                frame[y][x].ch = newCh;
            }
        }
    }

    void DrawLayoutDividers(std::shared_ptr<LayoutNode> node, std::vector<std::vector<Cell>>& frame, std::shared_ptr<Pane> activePane) {
        if (!node || node->split == SplitType::None) return;

        bool highlight = false;
        if (activePane) {
            Rect ab = activePane->bounds;
            Rect br = node->borderRect;
            if (node->split == SplitType::Vertical) {
                if (ab.x + ab.width == br.x || ab.x == br.x + 1) {
                    highlight = true;
                }
            } else if (node->split == SplitType::Horizontal) {
                if (ab.y + ab.height == br.y || ab.y == br.y + 1) {
                    highlight = true;
                }
            }
        }

        const UiTheme& theme = Theme();
        Color borderCol = highlight ? theme.activeBorder : theme.border;
        Rect br = node->borderRect;

        if (node->split == SplitType::Vertical) {
            for (int y = br.y; y < br.y + br.height && y < termH; ++y) {
                if (br.x >= 0 && br.x < termW) {
                    frame[y][br.x] = Cell{ L'|', borderCol, Color::DefaultBg() };
                }
            }
        } else if (node->split == SplitType::Horizontal) {
            for (int x = br.x; x < br.x + br.width && x < termW; ++x) {
                if (br.y >= 0 && br.y < termH) {
                    frame[br.y][x] = Cell{ L'-', borderCol, Color::DefaultBg() };
                }
            }
        }

        DrawLayoutDividers(node->first, frame, activePane);
        DrawLayoutDividers(node->second, frame, activePane);
    }

    void RenderFrame() {
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

    void HandleMouseEvent(const MOUSE_EVENT_RECORD& mer) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        int mx = mer.dwMousePosition.X;
        int my = mer.dwMousePosition.Y;
        auto& win = windows[activeWindowIdx];

        if (my == termH - 1 && (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
            int currentX = 4;
            for (size_t i = 0; i < windows.size(); ++i) {
                int len = (int)windows[i]->name.size() + 5;
                if (mx >= currentX && mx < currentX + len) {
                    activeWindowIdx = i;
                    RecalculateLayout();
                    break;
                }
                currentX += len;
            }
            return;
        }

        // Handle Border Dragging
        if (draggingBorderNode) {
            if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                if (draggingBorderNode->split == SplitType::Vertical) {
                    float r = (float)(mx - draggingBorderNode->x) / (float)(draggingBorderNode->w > 0 ? draggingBorderNode->w : 1);
                    draggingBorderNode->ratio = std::clamp(r, 0.1f, 0.9f);
                } else if (draggingBorderNode->split == SplitType::Horizontal) {
                    float r = (float)(my - draggingBorderNode->y) / (float)(draggingBorderNode->h > 0 ? draggingBorderNode->h : 1);
                    draggingBorderNode->ratio = std::clamp(r, 0.1f, 0.9f);
                }
                RecalculateLayout();
            } else {
                draggingBorderNode = nullptr;
            }
            return;
        }

        if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
            auto hitBorder = win->rootLayout->HitTestBorder(mx, my);
            if (hitBorder) {
                draggingBorderNode = hitBorder;
                return;
            }
        }

        // Find Target Pane
        std::shared_ptr<Pane> targetPane = nullptr;
        for (auto& p : win->GetPanes()) {
            if (p->bounds.Contains(mx, my)) {
                targetPane = p;
                break;
            }
        }

        if (!targetPane) return;

        if (win->activePane != targetPane && (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
            win->activePane = targetPane;
            MarkDirty();
        }

        // In-App Mouse Passthrough into Child Process (Lazygit, Micro, Far, Vim)
        if (targetPane->screen.mouseTracking && targetPane->screen.mouseSgrMode) {
            int relX = mx - targetPane->bounds.x + 1; // 1-based coordinates
            int relY = my - targetPane->bounds.y + 1;

            if (relX >= 1 && relX <= targetPane->screen.width && relY >= 1 && relY <= targetPane->screen.height) {
                int btn = 0;
                bool isRelease = false;

                if (mer.dwEventFlags == MOUSE_WHEELED) {
                    short wheel = (short)HIWORD(mer.dwButtonState);
                    btn = (wheel > 0) ? 64 : 65;
                } else if (mer.dwEventFlags == MOUSE_MOVED) {
                    if (!targetPane->screen.mouseAllMotion && !targetPane->screen.mouseButtonMotion) {
                        return;
                    }
                    if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                        btn = 32 + 0;
                    } else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                        btn = 32 + 2;
                    } else if (mer.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
                        btn = 32 + 1; // Middle button
                    } else {
                        if (targetPane->screen.mouseAllMotion) {
                            btn = 35; // Move with no buttons pressed
                        } else {
                            return;
                        }
                    }
                } else {
                    if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) btn = 0;
                    else if (mer.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) btn = 1;
                    else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED) btn = 2;
                    else isRelease = true;
                }

                std::string sgrSeq = "\x1b[<" + std::to_string(btn) + ";" + std::to_string(relX) + ";" + std::to_string(relY) + (isRelease ? "m" : "M");
                targetPane->SendInput(sgrSeq);
                return;
            }
        }

        // Default Scrollback for Multiplexer
        if (mer.dwEventFlags == MOUSE_WHEELED) {
            short wheel = (short)HIWORD(mer.dwButtonState);
            std::lock_guard<std::mutex> lock(targetPane->screen.mtx);
            if (wheel > 0) targetPane->screen.scrollOffset = std::min((int)targetPane->screen.scrollback.size(), targetPane->screen.scrollOffset + 3);
            else targetPane->screen.scrollOffset = std::max(0, targetPane->screen.scrollOffset - 3);
            MarkDirty();
        }
    }

    void MoveFocus(int dirKey) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto& win = windows[activeWindowIdx];
        if (!win || !win->activePane || win->isZoomed) return;

        auto activePane = win->activePane;
        Rect ab = activePane->bounds;

        std::shared_ptr<Pane> bestPane = nullptr;
        int bestDist = 999999;

        for (auto& p : win->GetPanes()) {
            if (p == activePane) continue;
            Rect pb = p->bounds;

            bool isCandidate = false;
            int dist = 0;

            if (dirKey == VK_LEFT) {
                if (pb.x + pb.width <= ab.x + 1) {
                    isCandidate = true;
                    dist = (ab.x - (pb.x + pb.width)) * 10 + std::abs((pb.y + pb.height/2) - (ab.y + ab.height/2));
                }
            } else if (dirKey == VK_RIGHT) {
                if (pb.x >= ab.x + ab.width - 1) {
                    isCandidate = true;
                    dist = (pb.x - (ab.x + ab.width)) * 10 + std::abs((pb.y + pb.height/2) - (ab.y + ab.height/2));
                }
            } else if (dirKey == VK_UP) {
                if (pb.y + pb.height <= ab.y + 1) {
                    isCandidate = true;
                    dist = (ab.y - (pb.y + pb.height)) * 10 + std::abs((pb.x + pb.width/2) - (ab.x + ab.width/2));
                }
            } else if (dirKey == VK_DOWN) {
                if (pb.y >= ab.y + ab.height - 1) {
                    isCandidate = true;
                    dist = (pb.y - (ab.y + ab.height)) * 10 + std::abs((pb.x + pb.width/2) - (ab.x + ab.width/2));
                }
            }

            if (isCandidate && dist < bestDist) {
                bestDist = dist;
                bestPane = p;
            }
        }

        if (bestPane) {
            win->activePane = bestPane;
            MarkDirty();
        }
    }
};

// ============================================================================
// [10] CLI Parser, Help & Entry Point
// ============================================================================
std::string GetHomeDirectory() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) return userProfile;

    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (homeDrive && homePath && *homeDrive && *homePath) return std::string(homeDrive) + homePath;

    const char* home = std::getenv("HOME");
    if (home && *home) return home;

    return ".";
}

std::string GetTmuxRcPath() {
    std::string home = GetHomeDirectory();
    if (!home.empty() && home.back() != '\\' && home.back() != '/') home += "\\";
    return home + ".tmuxrc";
}

void EnsureTmuxRcExists() {
    std::string path = GetTmuxRcPath();
    std::ifstream existing(path);
    if (existing.good()) return;

    std::ofstream rc(path);
    if (!rc.is_open()) return;
    rc << "# cmd-extended tmux startup configuration\n"
       << "# This file is created automatically so you can customize local keys.\n"
    << "# Available themes: blue, black, orange, silver, pink, yellow\n"
    << "# PowerShell is the default shell; uncomment shell to override.\n"
    << "# shell cmd.exe\n"
       << "theme blue\n"
       << "# Default avoids Windows-reserved shortcuts such as Win+key and Alt+Tab.\n"
    << "prefix-key Ctrl+B\n"
       << "theme-key Ctrl+Alt+T\n"
    << "bind split-vertical Alt+V\n"
    << "bind split-horizontal Alt+H\n"
    << "bind new-window Alt+T\n"
    << "bind next-window Alt+N\n"
    << "bind previous-window Alt+P\n"
    << "bind zoom Alt+Z\n"
    << "bind kill-pane Alt+W\n"
    << "bind kill-pane-alt Alt+Q\n"
    << "bind exit Ctrl+Alt+Q\n"
    << "# Also accepted: bind theme Ctrl+Alt+F8\n";
}

void LoadTmuxRc(WmuxEngine& mux) {
    std::ifstream rc(GetTmuxRcPath());
    if (!rc.is_open()) return;

    std::string line;
    while (std::getline(rc, line)) {
        size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line = TrimCopy(line);
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;
        std::string value;
        std::getline(ss, value);
        value = TrimCopy(value);
        command = LowerCopy(command);

        if (command == "shell" || command == "default-shell") {
            if (!value.empty()) mux.defaultShell = StringToWString(value);
        } else if (command == "theme") {
            mux.SetTheme(value);
        } else if (command == "prefix-key") {
            KeyCombo parsed;
            if (ParseKeyCombo(value, parsed)) mux.keys.prefixKey = parsed;
        } else if (command == "theme-key" || command == "bind-theme-key") {
            KeyCombo parsed;
            if (ParseKeyCombo(value, parsed)) mux.keys.themeCycle = parsed;
        } else if (command == "bind") {
            std::stringstream bindArgs(value);
            std::string action;
            bindArgs >> action;
            std::string keySpec;
            std::getline(bindArgs, keySpec);
            KeyCombo* target = FindHelperKey(mux.keys, action);
            KeyCombo parsed;
            if (target && ParseKeyCombo(keySpec, parsed)) *target = parsed;
        }
    }
}

void PrintHelp() {
    std::cout << R"(tmux(1)                 CrossShell for UNIX Reference Manual                 tmux(1)

    NAME
        tmux - terminal multiplexer

    SYNOPSIS
        tmux [OPTIONS] [COMMAND]

    DESCRIPTION
        tmux is a terminal multiplexer that enables multiple terminal sessions
        to be accessed and controlled concurrently from a single window.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    KEYBINDINGS
        GLOBAL ALT HOTKEYS (Fast, No Prefix Key Needed):
        Alt+v               Split active pane vertically (Left / Right)
        Alt+h               Split active pane horizontally (Top / Bottom)
        Alt+<Arrow Keys>    Navigate focus between adjacent panes
        Alt+z               Toggle zoom / full-screen on active pane
        Alt+w / Alt+q       Kill active pane
        Alt+t               Create new window tab
        Alt+n / Alt+p       Switch to next / previous window tab
        Ctrl+Alt+t          Cycle color theme (blue, black, orange, silver, pink, yellow)
        Ctrl+Alt+q          Exit / Detach tmux session

        PREFIX HOTKEYS (Press Ctrl+b, release, then press command key):
        Ctrl+b %            Split active pane vertically (Left / Right)
        Ctrl+b "            Split active pane horizontally (Top / Bottom)
        Ctrl+b <Arrow Keys> Navigate focus between adjacent panes
        Ctrl+b o            Cycle focus to next pane
        Ctrl+b z            Toggle zoom / full-screen on active pane
        Ctrl+b x            Kill active pane
        Ctrl+b c            Create new window tab
        Ctrl+b n / p        Switch to next / previous window tab
        Ctrl+b 0..9         Jump directly to window tab by index
        Ctrl+b ,            Rename current window tab
        Ctrl+b &            Kill current window tab
        Ctrl+b [            Scrollback mode (PgUp/PgDn/Arrows, 'q' to exit)
        Ctrl+b ]            Paste Windows OS clipboard to active pane
        Ctrl+b ?            Toggle interactive help overlay
        Ctrl+b d            Detach / Exit tmux session

        MOUSE SUPPORT & SGR PASSTHROUGH:
        Click               Focus pane / In-App clicking (Vim/Lazygit/Micro)
        Drag Border         Resize split panes dynamically
        Mouse Wheel         Scroll through history / in-app wheel reporting

    CONFIGURATION
        On startup tmux creates %USERPROFILE%\.tmuxrc if it is missing.
        Supported lines include:
            shell powershell.exe
            theme blue|black|orange|silver|pink|yellow
            prefix-key Ctrl+B
            theme-key Ctrl+Alt+T
            bind split-vertical Alt+V
            bind split-horizontal Alt+H
            bind new-window Alt+T
            bind next-window Alt+N
            bind previous-window Alt+P
            bind zoom Alt+Z
            bind kill-pane Alt+W
            bind kill-pane-alt Alt+Q
            bind exit Ctrl+Alt+Q

    EXAMPLES
        tmux
            Start a new tmux session with default shell.

        tmux cmd.exe
            Start a tmux session running cmd.exe.

    CrossShell for UNIX                                                      tmux(1)
)";
}

int main(int argc, char* argv[]) {
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