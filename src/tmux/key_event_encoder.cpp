#include "key_event_encoder.hpp"

std::string EncodeKeyEvent(const KEY_EVENT_RECORD& ker) {
    if (!ker.bKeyDown) return "";

    char ch = ker.uChar.AsciiChar;
    WORD vk = ker.wVirtualKeyCode;
    DWORD ctrl = ker.dwControlKeyState;
    bool isShift = (ctrl & SHIFT_PRESSED) != 0;
    bool isCtrl  = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    bool isAlt   = (ctrl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;

    if (ch != 0 && !isAlt && vk != VK_TAB && vk != VK_RETURN && vk != VK_BACK && vk != VK_ESCAPE) {
        return std::string(1, ch);
    }

    if (vk == VK_RETURN) return "\r";
    if (vk == VK_BACK)   return "\x7f";
    if (vk == VK_ESCAPE) return "\x1b";
    if (vk == VK_TAB) {
        if (isShift) return "\x1b[Z";
        return "\t";
    }

    if (vk >= VK_F1 && vk <= VK_F12) {
        int f = vk - VK_F1 + 1;
        if (f <= 4) {
            const char* fmap[] = { "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS" };
            return fmap[f - 1];
        } else {
            const int codeMap[] = { 15, 17, 18, 19, 20, 21, 23, 24 };
            return "\x1b[" + std::to_string(codeMap[f - 5]) + "~";
        }
    }

    int mod = 1;
    if (isShift) mod += 1;
    if (isAlt)   mod += 2;
    if (isCtrl)  mod += 4;

    auto formatCsi = [mod](char finalCh, const std::string& baseTilde = "") -> std::string {
        if (mod > 1) {
            if (!baseTilde.empty()) return "\x1b[" + baseTilde + ";" + std::to_string(mod) + "~";
            return "\x1b[1;" + std::to_string(mod) + finalCh;
        }
        if (!baseTilde.empty()) return "\x1b[" + baseTilde + "~";
        return std::string("\x1b[") + finalCh;
    };

    switch (vk) {
    case VK_UP:    return formatCsi('A');
    case VK_DOWN:  return formatCsi('B');
    case VK_RIGHT: return formatCsi('C');
    case VK_LEFT:  return formatCsi('D');
    case VK_HOME:  return formatCsi('H', "1");
    case VK_INSERT:return formatCsi(' ', "2");
    case VK_DELETE:return formatCsi(' ', "3");
    case VK_END:   return formatCsi('F', "4");
    case VK_PRIOR: return formatCsi(' ', "5");
    case VK_NEXT:  return formatCsi(' ', "6");
    }

    if (isAlt && ch != 0) return std::string("\x1b") + ch;
    return "";
}
