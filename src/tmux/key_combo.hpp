#pragma once

#include "tmux.hpp"

struct KeyCombo {
    bool ctrl = true;
    bool alt = true;
    bool shift = false;
    WORD vk = 'T';

    bool Matches(const KEY_EVENT_RECORD& ker) const;

    std::string Display() const;
};
