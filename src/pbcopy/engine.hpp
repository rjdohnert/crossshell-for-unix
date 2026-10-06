#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pbcopy.hpp"
#include "options.hpp"

class ClipboardManager {
public:
    static bool SetClipboardText(const std::wstring& text);
};

#endif // ENGINE_HPP
