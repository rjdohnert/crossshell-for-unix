#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// RAII Win32 Clipboard Wrappers
// ============================================================================

class ScopedClipboard {
public:
    explicit ScopedClipboard(HWND owner = nullptr, int retries = 5, DWORD delayMs = 10) 
        : isOpen_(false) {
        for (int i = 0; i < retries; ++i) {
            if (OpenClipboard(owner)) {
                isOpen_ = true;
                break;
            }
            Sleep(delayMs);
        }
    }

    ~ScopedClipboard() {
        if (isOpen_) {
            CloseClipboard();
        }
    }

    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    ScopedClipboard(const ScopedClipboard&) = delete;
    ScopedClipboard& operator=(const ScopedClipboard&) = delete;

private:
    bool isOpen_;
};

template <typename T>
class ScopedGlobalLock {
public:
    explicit ScopedGlobalLock(HGLOBAL handle)
        : handle_(handle), ptr_(handle ? static_cast<T*>(GlobalLock(handle)) : nullptr) {}

    ~ScopedGlobalLock() {
        if (ptr_) {
            GlobalUnlock(handle_);
        }
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    ScopedGlobalLock(const ScopedGlobalLock&) = delete;
    ScopedGlobalLock& operator=(const ScopedGlobalLock&) = delete;

private:
    HGLOBAL handle_;
    T* ptr_;
};

// ============================================================================
// Configuration & Command-Line Parser
// ============================================================================

enum class PreferredFormat {
    Text,       // Plain text (CF_UNICODETEXT)
    Rtf,        // Rich Text Format
    Html,       // HTML Format
    Files       // Drop files / file paths (CF_HDROP)
};

enum class LineEnding {
    Preserve,   // Keep clipboard line endings untouched
    Unix,       // Force LF (\n)
    Windows     // Force CRLF (\r\n)
};

struct AppOptions {
    PreferredFormat prefer = PreferredFormat::Text;
    LineEnding lineEnding = LineEnding::Preserve;
    std::string pboard = "general"; // macOS compat: general, ruler, find, font
    bool noNewline = false;
    bool raw = false;
    bool showHelp = false;
    bool showVersion = false;
};

class CommandLineParser {
public:
    static std::optional<AppOptions> parse(int argc, char* argv[]) {
        AppOptions opts;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                opts.showHelp = true;
                return opts;
            } else if (arg == "-V" || arg == "-v" || arg == "--version") {
                opts.showVersion = true;
                return opts;
            } else if (arg == "-n" || arg == "--no-newline") {
                opts.noNewline = true;
            } else if (arg == "-r" || arg == "--raw") {
                opts.raw = true;
            } else if (arg == "-u" || arg == "--unix") {
                opts.lineEnding = LineEnding::Unix;
            } else if (arg == "-w" || arg == "--windows" || arg == "--dos") {
                opts.lineEnding = LineEnding::Windows;
            } else if (arg == "-Prefer" || arg == "--prefer" || arg == "-prefer") {
                if (i + 1 >= argc) {
                    std::cerr << "pbpaste: error: missing argument for " << arg << "\n";
                    return std::nullopt;
                }
                std::string fmt = argv[++i];
                std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::tolower);

                if (fmt == "txt" || fmt == "text") {
                    opts.prefer = PreferredFormat::Text;
                } else if (fmt == "rtf") {
                    opts.prefer = PreferredFormat::Rtf;
                } else if (fmt == "html") {
                    opts.prefer = PreferredFormat::Html;
                } else if (fmt == "files" || fmt == "file") {
                    opts.prefer = PreferredFormat::Files;
                } else {
                    std::cerr << "pbpaste: error: unknown format '" << fmt 
                              << "' (choose: txt, rtf, html, files)\n";
                    return std::nullopt;
                }
            } else if (arg == "-pboard") {
                // Maintained for macOS flag compatibility
                if (i + 1 >= argc) {
                    std::cerr << "pbpaste: error: missing argument for -pboard\n";
                    return std::nullopt;
                }
                opts.pboard = argv[++i];
            } else {
                std::cerr << "pbpaste: unrecognized option: " << arg << "\n";
                std::cerr << "Try 'pbpaste --help' for more information.\n";
                return std::nullopt;
            }
        }

        return opts;
    }

    static void printHelp(const char* progName) {
        std::cout 
            << "NAME\n"
            << "    pbpaste - paste text and data from the Windows clipboard to stdout\n\n"
            << "SYNOPSIS\n"
            << "    " << progName << " [OPTIONS]\n\n"
            << "DESCRIPTION\n"
            << "    It extracts text, formatted data, or copied file lists from the Windows\n"
            << "    system clipboard and writes it to standard output in UTF-8 encoding.\n\n"
            << "OPTIONS\n"
            << "    -Prefer, --prefer <format>\n"
            << "        Specifies what data format to pull from the clipboard.\n"
            << "        Supported formats:\n"
            << "            txt     Plain text (Unicode) [Default]\n"
            << "            rtf     Rich Text Format raw source markup\n"
            << "            html    HTML source markup copied to the clipboard\n"
            << "            files   List of file paths if files were copied via Explorer\n\n"
            << "    -pboard <name>\n"
            << "        Specifies pasteboard name (general, find, ruler, font).\n"
            << "        Windows supports a single system-wide clipboard; this flag is\n"
            << "        accepted for seamless macOS script compatibility.\n\n"
            << "    -n, --no-newline\n"
            << "        Do not output a trailing newline character.\n\n"
            << "    -u, --unix\n"
            << "        Convert Windows CRLF (\\r\\n) line endings to Unix LF (\\n).\n\n"
            << "    -w, --windows\n"
            << "        Ensure Windows CRLF (\\r\\n) line endings.\n\n"
            << "    -r, --raw\n"
            << "        Output clipboard payload as raw bytes without sanitization.\n\n"
            << "    -h, --help\n"
            << "        Display this help message and exit.\n\n"
            << "    -v, -V, --version\n"
            << "        Display version information and exit.\n\n"
            << "EXAMPLES\n"
            << "    # Print clipboard contents to terminal:\n"
            << "        pbpaste\n\n"
            << "    # Filter clipboard text through grep into a file:\n"
            << "        pbpaste | grep \"TODO\" > tasks.txt\n\n"
            << "    # Paste HTML source code directly to a file:\n"
            << "        pbpaste -Prefer html > snippet.html\n\n"
            << "    # Paste copied Explorer files as a newline-delimited path list:\n"
            << "        pbpaste -Prefer files\n\n"
            << "    # Normalize line endings to Unix style for a bash script:\n"
            << "        pbpaste -u > script.sh\n";
    }

    static void printVersion() {
        std::cout << "pbpaste 1.0.0\n"
                  << "Copyright (c) 2026, Roberto J Dohnert\n";
    }
};

// ============================================================================
// Clipboard Manager
// ============================================================================

class ClipboardManager {
public:
    ClipboardManager() {
        cfRtf_ = RegisterClipboardFormatW(L"Rich Text Format");
        cfHtml_ = RegisterClipboardFormatW(L"HTML Format");
    }

    [[nodiscard]] std::optional<std::string> read(PreferredFormat format) const {
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

private:
    UINT cfRtf_;
    UINT cfHtml_;

    [[nodiscard]] std::optional<std::string> readUnicodeText() const {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            return std::nullopt;
        }

        HGLOBAL hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData) return std::nullopt;

        ScopedGlobalLock<const wchar_t> lock(hData);
        if (!lock) return std::nullopt;

        return utf16ToUtf8(lock.get());
    }

    [[nodiscard]] std::optional<std::string> readCustomFormat(UINT formatId) const {
        if (formatId == 0 || !IsClipboardFormatAvailable(formatId)) {
            return std::nullopt;
        }

        HGLOBAL hData = GetClipboardData(formatId);
        if (!hData) return std::nullopt;

        ScopedGlobalLock<const char> lock(hData);
        if (!lock) return std::nullopt;

        return std::string(lock.get());
    }

    [[nodiscard]] std::optional<std::string> readDropFiles() const {
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

    static std::string utf16ToUtf8(const wchar_t* utf16Str) {
        if (!utf16Str || *utf16Str == L'\0') return "";

        int bytesNeeded = WideCharToMultiByte(
            CP_UTF8, 0, utf16Str, -1, nullptr, 0, nullptr, nullptr);

        if (bytesNeeded <= 1) return "";

        std::string utf8Str(static_cast<size_t>(bytesNeeded) - 1, '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, utf16Str, -1, utf8Str.data(), bytesNeeded, nullptr, nullptr);

        return utf8Str;
    }
};

// ============================================================================
// Text Processor & Output Formatter
// ============================================================================

class TextFormatter {
public:
    static std::string format(std::string text, const AppOptions& opts) {
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

private:
    static std::string toUnixLineEndings(const std::string& input) {
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

    static std::string toWindowsLineEndings(const std::string& input) {
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
};

// ============================================================================
// Main Application Controller
// ============================================================================

class PbPasteApp {
public:
    int run(int argc, char* argv[]) {
        auto opts = CommandLineParser::parse(argc, argv);
        if (!opts) {
            return 1;
        }

        if (opts->showHelp) {
            CommandLineParser::printHelp(argv[0]);
            return 0;
        }

        if (opts->showVersion) {
            CommandLineParser::printVersion();
            return 0;
        }

        // Ensure stdout is opened in binary mode to prevent Windows CRT
        // runtime from converting \n to \r\n automatically
        _setmode(_fileno(stdout), _O_BINARY);

        ClipboardManager clipboard;
        auto content = clipboard.read(opts->prefer);

        // Fallback: If RTF or HTML was requested but unavailable, fall back to plain text
        if (!content && opts->prefer != PreferredFormat::Text) {
            content = clipboard.read(PreferredFormat::Text);
        }

        if (!content || content->empty()) {
            return 0; // Empty clipboard returns 0 matching macOS behavior
        }

        std::string output = TextFormatter::format(std::move(*content), *opts);
        std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
        std::cout.flush();

        return 0;
    }
};

int main(int argc, char* argv[]) {
    PbPasteApp app;
    return app.run(argc, argv);
}