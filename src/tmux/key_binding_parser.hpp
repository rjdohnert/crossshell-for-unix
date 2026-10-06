#pragma once

#include "helper_keys.hpp"
#include "key_combo.hpp"
#include "tmux.hpp"

KeyCombo* FindHelperKey(HelperKeys& keys, const std::string& action);

bool ParseKeyCombo(const std::string& value, KeyCombo& combo);
