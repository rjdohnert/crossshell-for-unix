#include "color.hpp"

Color Color::DefaultFg() { Color c; c.r = 204; c.g = 204; c.b = 204; c.isDefault = true; return c; }

Color Color::DefaultBg() { Color c; c.r = 12;  c.g = 12;  c.b = 12;  c.isDefault = true; return c; }

Color Color::Rgb(uint8_t r, uint8_t g, uint8_t b) { Color c; c.r = r; c.g = g; c.b = b; c.isDefault = false; return c; }

Color Color::Idx(uint8_t i) { Color c; c.index = i; c.isIndexed = true; c.isDefault = false; return c; }

bool Color::operator==(const Color& o) const {
        if (isDefault && o.isDefault) return true;
        if (isDefault != o.isDefault) return false;
        if (isIndexed && o.isIndexed) return index == o.index;
        return r == o.r && g == o.g && b == o.b;
    }

bool Color::operator!=(const Color& o) const { return !(*this == o); }
