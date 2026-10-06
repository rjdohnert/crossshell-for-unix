#include "options.hpp"

std::optional<AppOptions> CommandLineParser::parse(int argc, char* argv[]) {
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

void CommandLineParser::printHelp(const char* progName) {
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

void CommandLineParser::printVersion() {
    std::cout << "pbpaste 1.0.0\n"
              << "Copyright (c) 2026, Roberto J Dohnert\n";
}
