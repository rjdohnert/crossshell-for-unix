#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pbpaste.hpp"
#include "options.hpp"

class ClipboardManager {
public:
    ClipboardManager();
    [[nodiscard]] std::optional<std::string> read(PreferredFormat format) const;

private:
    UINT cfRtf_;
    UINT cfHtml_;

    [[nodiscard]] std::optional<std::string> readUnicodeText() const;
    [[nodiscard]] std::optional<std::string> readCustomFormat(UINT formatId) const;
    [[nodiscard]] std::optional<std::string> readDropFiles() const;
    static std::string utf16ToUtf8(const wchar_t* utf16Str);
};

class TextFormatter {
public:
    static std::string format(std::string text, const AppOptions& opts);

private:
    static std::string toUnixLineEndings(const std::string& input);
    static std::string toWindowsLineEndings(const std::string& input);
};

#endif // ENGINE_HPP
