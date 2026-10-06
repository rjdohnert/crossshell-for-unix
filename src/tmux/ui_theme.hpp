#pragma once

#include "color.hpp"
#include "tmux.hpp"

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
