#include "engine.hpp"

// ============================================================================
// ClipboardManager Implementation
// ============================================================================

ClipboardManager::ClipboardManager() {
    cfRtf_ = RegisterClipboardFormatW(L"Rich Text Format");
    cfHtml_ = RegisterClipboardFormatW(L"HTML Format");
}

std::optional<std::string> ClipboardManager::read(PreferredFormat format) const {
    ScopedClipboard clip;
    if (!clip.isOpen()) {
        std::cerr << "pbpaste: error: unable to open clipboard.\n";
        return std::nullopt;
    }

    switch (format) {
        case PreferredFormat::Text:
            return readUnicodeText();
        case PreferredFormat::Rtf:
            return readCustomFormat(cfRtf_);
        case PreferredFormat::Html:
            return readCustomFormat(cfHtml_);
        case PreferredFormat::Files:
            return readDropFiles();
    }
    return std::nullopt;
}

std::optional<std::string> ClipboardManager::readUnicodeText() const {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return std::nullopt;
    }

    HGLOBAL hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) return std::nullopt;

    ScopedGlobalLock<const wchar_t> lock(hData);
    if (!lock) return std::nullopt;

    return utf16ToUtf8(lock.get());
}

std::optional<std::string> ClipboardManager::readCustomFormat(UINT formatId) const {
    if (formatId == 0 || !IsClipboardFormatAvailable(formatId)) {
        return std::nullopt;
    }

    HGLOBAL hData = GetClipboardData(formatId);
    if (!hData) return std::nullopt;

    ScopedGlobalLock<const char> lock(hData);
    if (!lock) return std::nullopt;

    return std::string(lock.get());
}

std::optional<std::string> ClipboardManager::readDropFiles() const {
    if (!IsClipboardFormatAvailable(CF_HDROP)) {
        return std::nullopt;
    }

    HGLOBAL hData = GetClipboardData(CF_HDROP);
    if (!hData) return std::nullopt;

    ScopedGlobalLock<DROPFILES> lock(hData);
    if (!lock) return std::nullopt;

    auto hDrop = reinterpret_cast<HDROP>(hData);
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (fileCount == 0) return std::nullopt;

    std::string result;
    std::vector<wchar_t> buffer(MAX_PATH);

    for (UINT i = 0; i < fileCount; ++i) {
        UINT length = DragQueryFileW(hDrop, i, nullptr, 0);
        if (length >= buffer.size()) {
            buffer.resize(length + 1);
        }
        DragQueryFileW(hDrop, i, buffer.data(), static_cast<UINT>(buffer.size()));
        
        result += utf16ToUtf8(buffer.data());
        result += "\n";
    }

    return result;
}

std::string ClipboardManager::utf16ToUtf8(const wchar_t* utf16Str) {
    if (!utf16Str || *utf16Str == L'\0') return "";

    int bytesNeeded = WideCharToMultiByte(
        CP_UTF8, 0, utf16Str, -1, nullptr, 0, nullptr, nullptr);

    if (bytesNeeded <= 1) return "";

    std::string utf8Str(static_cast<size_t>(bytesNeeded) - 1, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, utf16Str, -1, utf8Str.data(), bytesNeeded, nullptr, nullptr);

    return utf8Str;
}

// ============================================================================
// TextFormatter Implementation
// ============================================================================

std::string TextFormatter::format(std::string text, const AppOptions& opts) {
    if (opts.raw) {
        return text;
    }

    // Handle line ending normalization
    if (opts.lineEnding == LineEnding::Unix) {
        text = toUnixLineEndings(text);
    } else if (opts.lineEnding == LineEnding::Windows) {
        text = toWindowsLineEndings(text);
    }

    // Handle trailing newlines
    if (opts.noNewline) {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
        }
    }

    return text;
}

std::string TextFormatter::toUnixLineEndings(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                out.push_back('\n');
                ++i;
            } else {
                out.push_back('\n');
            }
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

std::string TextFormatter::toWindowsLineEndings(const std::string& input) {
    std::string out;
    out.reserve(input.size() + input.size() / 10);
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                out.append("\r\n");
                ++i;
            } else {
                out.append("\r\n");
            }
        } else if (input[i] == '\n') {
            out.append("\r\n");
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}
