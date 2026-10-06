#pragma once

#include "tmux.hpp"

struct Rect {
    int x = 0, y = 0, width = 0, height = 0;
    bool Contains(int px, int py) const;
};
