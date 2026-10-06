#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pbpaste.hpp"

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
    static std::optional<AppOptions> parse(int argc, char* argv[]);
    static void printHelp(const char* progName);
    static void printVersion();
};

#endif // OPTIONS_HPP
