#include "helper_keys.hpp"
#include "key_binding_parser.hpp"
#include "key_combo.hpp"
#include "string_utils.hpp"

KeyCombo* FindHelperKey(HelperKeys& keys, const std::string& action) {
    std::string name = LowerCopy(TrimCopy(action));
    if (name == "prefix" || name == "prefix-key") return &keys.prefixKey;
    if (name == "split-vertical" || name == "splitv") return &keys.splitVertical;
    if (name == "split-horizontal" || name == "splith") return &keys.splitHorizontal;
    if (name == "new-window" || name == "new-tab") return &keys.newWindow;
    if (name == "next-window" || name == "next-tab") return &keys.nextWindow;
    if (name == "previous-window" || name == "prev-window" || name == "previous-tab" || name == "prev-tab") return &keys.previousWindow;
    if (name == "zoom" || name == "zoom-pane") return &keys.zoomPane;
    if (name == "kill-pane" || name == "kill") return &keys.killPane;
    if (name == "kill-pane-alt" || name == "kill-alternate" || name == "kill-secondary") return &keys.killPaneAlternate;
    if (name == "exit" || name == "detach" || name == "exit-session") return &keys.exitSession;
    if (name == "theme" || name == "theme-cycle" || name == "cycle-theme") return &keys.themeCycle;
    return nullptr;
}

bool ParseKeyCombo(const std::string& value, KeyCombo& combo) {
    KeyCombo parsed;
    parsed.ctrl = false;
    parsed.alt = false;
    parsed.shift = false;
    parsed.vk = 0;

    std::stringstream ss(value);
    std::string part;
    while (std::getline(ss, part, '+')) {
        std::string token = LowerCopy(TrimCopy(part));
        if (token.empty()) continue;
        if (token == "ctrl" || token == "control") parsed.ctrl = true;
        else if (token == "alt") parsed.alt = true;
        else if (token == "shift") parsed.shift = true;
        else if (token.size() == 1 && std::isalnum((unsigned char)token[0])) parsed.vk = (WORD)std::toupper((unsigned char)token[0]);
        else if (token == "left") parsed.vk = VK_LEFT;
        else if (token == "right") parsed.vk = VK_RIGHT;
        else if (token == "up") parsed.vk = VK_UP;
        else if (token == "down") parsed.vk = VK_DOWN;
        else if (token == "pgup" || token == "pageup") parsed.vk = VK_PRIOR;
        else if (token == "pgdn" || token == "pagedown") parsed.vk = VK_NEXT;
        else if (token.size() >= 2 && token[0] == 'f') {
            int fn = std::atoi(token.c_str() + 1);
            if (fn >= 1 && fn <= 12) parsed.vk = (WORD)(VK_F1 + fn - 1);
            else return false;
        } else {
            return false;
        }
    }

    if (parsed.vk == 0 || (!parsed.ctrl && !parsed.alt && !parsed.shift)) return false;
    combo = parsed;
    return true;
}
