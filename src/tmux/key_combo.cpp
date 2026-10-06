#include "key_combo.hpp"

bool KeyCombo::Matches(const KEY_EVENT_RECORD& ker) const {
        bool hasCtrl = (ker.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool hasAlt = (ker.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        bool hasShift = (ker.dwControlKeyState & SHIFT_PRESSED) != 0;
        return ker.wVirtualKeyCode == vk && hasCtrl == ctrl && hasAlt == alt && hasShift == shift;
    }

std::string KeyCombo::Display() const {
        std::string out;
        if (ctrl) out += "Ctrl+";
        if (alt) out += "Alt+";
        if (shift) out += "Shift+";
        if (vk >= VK_F1 && vk <= VK_F12) out += "F" + std::to_string(vk - VK_F1 + 1);
        else if (vk >= 'A' && vk <= 'Z') out += (char)vk;
        else out += "VK" + std::to_string(vk);
        return out;
    }
