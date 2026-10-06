#pragma once

#include "key_combo.hpp"
#include "tmux.hpp"

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
