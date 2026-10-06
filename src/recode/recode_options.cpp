#include "recode_options.hpp"

void RecodeOptions::showHelp() {
        std::cout << R"(recode(1)                  CrossShell for UNIX Reference Manual                 recode(1)

    NAME
        recode - convert files between character sets and surfaces

    SYNOPSIS
        recode [OPTIONS] REQUEST [FILE...]
        recode [OPTIONS] REQUEST < INPUT > OUTPUT

    DESCRIPTION
        recode converts character encodings and data surfaces between files or
        standard streams. It can transform between various code pages, UTF
        encodings, Base64, Hexadecimal, URL percent-encoding, HTML entities,
        and line endings.

    OPTIONS
        -l, --list
            List all supported character sets and surfaces.

        -v, --verbose
            Print each conversion step and transformation to standard error.

        -f, --force
            Force conversion even when characters cannot be represented exactly.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        recode ..base64 message.txt
            Base64 encode message.txt in place.

        recode base64.. < encoded.txt > decoded.bin
            Decode Base64 stream from standard input to standard output.

        recode l1..u8 document.txt
            Convert document.txt from Latin-1 (ISO-8859-1) to UTF-8.

        recode utf8..html < page.html > escaped.html
            Convert special characters in UTF-8 text to HTML entities.

        recode /rot13 < text.txt > cipher.txt
            Apply ROT13 cipher transformation to input text.

    CrossShell for UNIX                                                          recode(1)
)";
    }

void RecodeOptions::listSupported() {
        std::cout << "\nSupported Character Sets (Windows Native):\n";
        std::cout << "  - UTF-8 (u8, utf8, utf-8)\n";
        std::cout << "  - ISO-8859-1 (l1, latin1, iso-8859-1)\n";
        std::cout << "  - Windows-1252 (cp1252, win1252, ansi)\n";
        std::cout << "  - Windows-1251 (cp1251, cyrillic)\n";
        std::cout << "  - Windows-1250 (cp1250, Central European)\n";
        std::cout << "  - UTF-16LE (u16, utf16, utf-16le)\n";
        std::cout << "  - ASCII (ascii, us-ascii, us)\n";
        std::cout << "  - IBM-PC (cp437, dos, oem)\n";
        std::cout << "  - Shift-JIS (cp932, sjis, shift-jis)\n";
        std::cout << "\nSupported Surfaces:\n";
        std::cout << "  - Base64 (b64, base64)\n";
        std::cout << "  - Hexadecimal (hex, base16, 16)\n";
        std::cout << "  - URL Percent-Encoding (url, percent)\n";
        std::cout << "  - HTML Entities (html, entity)\n";
        std::cout << "  - ROT13 Substitution (/rot13, /13)\n";
        std::cout << "  - Line Endings (crlf, lf)\n\n";
    }

RecodeOptions RecodeOptions::parse(int argc, char* argv[]) {
        RecodeOptions opts;
        std::vector<std::string> args;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                showHelp();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opts.listCharsets = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
            } else {
                args.push_back(std::string(arg));
            }
        }

        if (opts.listCharsets) return opts;

        if (!args.empty()) {
            opts.request = args[0];
            for (size_t i = 1; i < args.size(); ++i) {
                opts.fileList.push_back(args[i]);
            }
        }

        return opts;
    }
