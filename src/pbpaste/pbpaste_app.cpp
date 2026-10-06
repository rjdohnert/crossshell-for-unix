#include "pbpaste_app.hpp"

int PbPasteApp::run(int argc, char* argv[]) {
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
