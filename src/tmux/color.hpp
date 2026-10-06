#pragma once

#include "tmux.hpp"

struct Color {
    uint8_t r = 204, g = 204, b = 204;
    bool isDefault = true;
    bool isIndexed = false;
    uint8_t index = 7;

    static Color DefaultFg();
    static Color DefaultBg();
    static Color Rgb(uint8_t r, uint8_t g, uint8_t b);
    static Color Idx(uint8_t i);

    bool operator==(const Color& o) const;
    bool operator!=(const Color& o) const;
};
