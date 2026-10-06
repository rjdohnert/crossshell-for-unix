#include "options.hpp"

void OdOptions::printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTION]... [FILE]...\n"
              << "Write an unambiguous representation, octal bytes by default,\n"
              << "of FILE to standard output. With no FILE, read standard input.\n\n"
              << "Options:\n"
              << "  -A, --address-radix=RADIX  decide how file offsets are printed (d, o, x, n)\n"
              << "  -j, --skip-bytes=BYTES     skip BYTES input bytes before formatting\n"
              << "  -N, --read-bytes=BYTES     limit dump to BYTES input bytes\n"
              << "  -v, --output-duplicates    do not use * to mark line suppression\n"
              << "  -w, --width[=BYTES]        format BYTES bytes per output line (default 16)\n"
              << "  -a                         same as -t a, select named characters\n"
              << "  -b                         same as -t o1, select octal bytes\n"
              << "  -c                         same as -t c, select printable chars or escapes\n"
              << "  -d                         same as -t u2, select unsigned decimal 2-byte units\n"
              << "  -o                         same as -t o2, select octal 2-byte units\n"
              << "  -x                         same as -t x2, select hexadecimal 2-byte units\n"
              << "  -t, --format=TYPE          select output format or type\n"
              << "      --json, --csv, --table structured output formats\n"
              << "      --pipe COMMAND         send formatted output through COMMAND\n"
              << "  -h, --help                 display this help and exit\n"
              << "      --version              output version information and exit\n";
}

void OdOptions::printVersion() {
    std::cout << "od 1.0.0\n";
}

uint64_t OdOptions::parseByteCount(const std::string& str) {
    size_t idx = 0;
    uint64_t val = std::stoull(str, &idx);
    if (idx < str.length()) {
        char suffix = str[idx];
        if (suffix == 'k' || suffix == 'K') val *= 1024;
        else if (suffix == 'm' || suffix == 'M') val *= 1024 * 1024;
        else if (suffix == 'g' || suffix == 'G') val *= 1024 * 1024 * 1024;
        else if (suffix == 'b') val *= 512;
    }
    return val;
}

bool OdOptions::parseTypeString(const std::string& typeStr, std::vector<FormatSpec>& formats) {
    if (typeStr.empty()) return false;
    size_t i = 0;
    while (i < typeStr.length()) {
        char code = typeStr[i++];
        size_t size = 4;

        if (code == 'a') {
            formats.push_back({ FormatKind::ASCII_NAMED, 1 });
            continue;
        } else if (code == 'c') {
            formats.push_back({ FormatKind::CHAR_ESCAPE, 1 });
            continue;
        }

        if (code == 'f') size = sizeof(double);
        else if (code == 'd' || code == 'u' || code == 'o' || code == 'x') size = sizeof(int);
        else return false;

        if (i < typeStr.length()) {
            char s = typeStr[i];
            if (s == 'C' || s == '1') { size = 1; i++; }
            else if (s == 'S' || s == '2') { size = 2; i++; }
            else if (s == 'I' || s == '4') { size = 4; i++; }
            else if (s == 'L' || s == '8') { size = 8; i++; }
            else if (s == 'F') { size = sizeof(float); i++; }
            else if (s == 'D') { size = sizeof(double); i++; }
        }

        switch (code) {
            case 'd': formats.push_back({ FormatKind::SIGNED_DEC, size }); break;
            case 'u': formats.push_back({ FormatKind::UNSIGNED_DEC, size }); break;
            case 'o': formats.push_back({ FormatKind::OCTAL, size }); break;
            case 'x': formats.push_back({ FormatKind::HEX, size }); break;
            case 'f': formats.push_back({ FormatKind::FLOAT, size }); break;
        }
    }
    return true;
}

bool OdOptions::parse(int argc, char* argv[], OdOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-v" || arg == "--output-duplicates") {
            opts.outputDuplicates = true;
        } else if (arg == "-a") {
            opts.formats.push_back({ FormatKind::ASCII_NAMED, 1 });
        } else if (arg == "-b") {
            opts.formats.push_back({ FormatKind::OCTAL, 1 });
        } else if (arg == "-c") {
            opts.formats.push_back({ FormatKind::CHAR_ESCAPE, 1 });
        } else if (arg == "-d") {
            opts.formats.push_back({ FormatKind::UNSIGNED_DEC, 2 });
        } else if (arg == "-o") {
            opts.formats.push_back({ FormatKind::OCTAL, 2 });
        } else if (arg == "-x") {
            opts.formats.push_back({ FormatKind::HEX, 2 });
        } else if (arg == "--json") {
            opts.outputFormat = 1;
        } else if (arg == "--csv") {
            opts.outputFormat = 2;
        } else if (arg == "--table") {
            opts.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (arg.rfind("-A", 0) == 0) {
            std::string r = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "o");
            if (!r.empty()) opts.addressRadix = r[0];
        } else if (arg.rfind("-j", 0) == 0) {
            std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "0");
            opts.skipBytes = parseByteCount(s);
        } else if (arg.rfind("-N", 0) == 0) {
            std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "0");
            opts.limitBytes = parseByteCount(s);
        } else if (arg.rfind("-w", 0) == 0) {
            std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "16");
            opts.bytesPerLine = static_cast<size_t>(parseByteCount(s));
        } else if (arg.rfind("-t", 0) == 0) {
            std::string t = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "o2");
            parseTypeString(t, opts.formats);
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::cerr << "od: unrecognized option '" << arg << "'\n";
            return false;
        } else {
            opts.filenames.push_back(arg);
        }
    }

    if (opts.formats.empty()) {
        opts.formats.push_back({ FormatKind::OCTAL, 2 });
    }

    if (opts.filenames.empty()) {
        opts.filenames.push_back("-");
    }

    return true;
}
