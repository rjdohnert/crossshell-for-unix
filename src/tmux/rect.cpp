#include "rect.hpp"

bool Rect::Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
