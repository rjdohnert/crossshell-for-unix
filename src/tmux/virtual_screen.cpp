#include "cell.hpp"
#include "code_point_width.hpp"
#include "color.hpp"
#include "virtual_screen.hpp"

VirtualScreen::VirtualScreen(int w, int h) : width(std::max(1, w)), height(std::max(1, h)) {
        scrollTop = 0;
        scrollBottom = height - 1;
        ResizeGrid(width, height);
    }

std::vector<std::vector<Cell>>& VirtualScreen::ActiveGrid() {
        return usingAltBuffer ? altGrid : primaryGrid;
    }

void VirtualScreen::ResizeGrid(int w, int h) {
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

void VirtualScreen::ScrollRegionUp(int top, int bottom) {
        auto& g = ActiveGrid();
        if (top < 0 || bottom >= height || top >= bottom) return;
        if (!usingAltBuffer && top == 0) {
            scrollback.push_back(g[top]);
            if (scrollback.size() > maxScrollback) scrollback.pop_front();
        }
        for (int y = top; y < bottom; ++y) g[y] = g[y + 1];
        g[bottom] = std::vector<Cell>(width, Cell{ L' ', curFg, curBg });
    }

void VirtualScreen::ScrollRegionDown(int top, int bottom) {
        auto& g = ActiveGrid();
        if (top < 0 || bottom >= height || top >= bottom) return;
        for (int y = bottom; y > top; --y) g[y] = g[y - 1];
        g[top] = std::vector<Cell>(width, Cell{ L' ', curFg, curBg });
    }

void VirtualScreen::PutCodePoint(uint32_t cp) {
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

void VirtualScreen::ParseSgr() {
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

void VirtualScreen::ExecuteCsi(char finalChar) {
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

void VirtualScreen::ProcessBytes(const char* data, size_t len) {
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
