#pragma once

#include "color.hpp"
#include "tmux.hpp"

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
         bool bl = false, bool dm = false, bool it = false, bool ul = false, bool inv = false, uint8_t w = 1);

    bool operator==(const Cell& o) const;
    bool operator!=(const Cell& o) const;
};
