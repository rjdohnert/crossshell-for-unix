#include "sequence_options.hpp"

bool KeyDefinition::parse(const std::string& def, KeyDefinition& outKey, const KeyFlags& globalFlags) {
    outKey.flags = globalFlags;
    size_t comma = def.find(',');
    std::string startPart = (comma == std::string::npos) ? def : def.substr(0, comma);
    std::string endPart = (comma == std::string::npos) ? "" : def.substr(comma + 1);

    auto parsePart = [](const std::string& p, int& f, int& c, KeyFlags& kflags, bool& custom) {
        size_t idx = 0;
        f = 0; c = 0;
        while (idx < p.size() && std::isdigit(static_cast<unsigned char>(p[idx]))) {
            f = f * 10 + (p[idx] - '0');
            ++idx;
        }
        if (f == 0) f = 1;
        if (idx < p.size() && p[idx] == '.') {
            ++idx;
            while (idx < p.size() && std::isdigit(static_cast<unsigned char>(p[idx]))) {
                c = c * 10 + (p[idx] - '0');
                ++idx;
            }
        }
        while (idx < p.size()) {
            custom = true;
            char opt = p[idx++];
            switch (opt) {
                case 'n': kflags.numeric = true; break;
                case 'h': kflags.humanNumeric = true; break;
                case 'g': kflags.generalNumeric = true; break;
                case 'M': kflags.month = true; break;
                case 'V': kflags.version = true; break;
                case 'r': kflags.reverse = true; break;
                case 'f': kflags.ignoreCase = true; break;
                case 'd': kflags.dictOrder = true; break;
                case 'i': kflags.ignoreNonPrint = true; break;
                case 'b': kflags.ignoreLeadingBlanks = true; break;
                default: break;
            }
        }
    };

    parsePart(startPart, outKey.startField, outKey.startChar, outKey.flags, outKey.hasCustomFlags);
    if (!endPart.empty()) {
        parsePart(endPart, outKey.endField, outKey.endChar, outKey.flags, outKey.hasCustomFlags);
    }
    return true;
}

void SequenceOptions::showHelp() {
    std::cout << R"(sequence(1)             CrossShell for UNIX Reference Manual                sequence(1)

    NAME
        sequence - sort concatenated input records

    SYNOPSIS
        sequence [OPTIONS] [FILE]...

    DESCRIPTION
        Reads files or standard input, concatenates the records, and writes sorted
        output. Windows streams are binary-configured; '-' denotes standard input.

    OPTIONS
        -b, --ignore-leading-blanks
            Ignore leading blanks.

        -d, --dictionary-order
            Use dictionary order.

        -f, --ignore-case
            Compare without case distinctions.

        -g, --general-numeric-sort
            Sort general numeric values.

        -h, --human-numeric-sort
            Sort human-readable numeric values.

        -i, --ignore-nonprinting
            Ignore nonprinting characters.

        -M, --month-sort
            Sort by month names.

        -n, --numeric-sort
            Sort numerically.

        -R, --random-sort
            Deterministically shuffle records.

        -r, --reverse
            Reverse the sort order.

        -V, --version-sort
            Sort version strings.

        -c, --check
            Check whether input is sorted.

        -C, --check=quiet
            Check silently.

        -k, --key KEYDEF
            Sort by a field/character key.

        -o, --output FILE
            Write sorted output to FILE.

        -s, --stable
            Enable stable sorting.

        -t, --field-separator SEP
            Set the field separator.

        -u, --unique
            Remove equal adjacent output keys.

        -z, --zero-terminated
            Use NUL instead of newline records.

        --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        sequence -u names.txt
            Sort names.txt removing duplicate lines.

        sequence -t: -k3,3n /etc/passwd
            Sort passwd entries numerically by third field.

        sequence -h -r -k2 disk_usage.txt -o sorted_usage.txt
            Sort disk usage by second field in human-numeric reverse order.

    EXIT STATUS
        0          Help, version, successful sorting, or ordered input.
        1          Check mode detected disorder.
        2          Invalid parsing or input/output file failure.

    CrossShell for UNIX                                                    sequence(1)
)";
}

void SequenceOptions::showVersion() {
    std::cout << "sequence 3.0.7\n";
    std::cout << "Licensed under the BSD-3-Clause License\n";
}

bool SequenceOptions::parse(int argc, char* argv[], SequenceOptions& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?" || arg == "/?") { showHelp(); std::exit(0); }
        if (arg == "--version") { showVersion(); std::exit(0); }

        if (arg.rfind("--", 0) == 0) {
            if (arg == "--ignore-leading-blanks") opt.globalFlags.ignoreLeadingBlanks = true;
            else if (arg == "--dictionary-order") opt.globalFlags.dictOrder = true;
            else if (arg == "--ignore-case") opt.globalFlags.ignoreCase = true;
            else if (arg == "--general-numeric-sort") opt.globalFlags.generalNumeric = true;
            else if (arg == "--human-numeric-sort") opt.globalFlags.humanNumeric = true;
            else if (arg == "--ignore-nonprinting") opt.globalFlags.ignoreNonPrint = true;
            else if (arg == "--month-sort") opt.globalFlags.month = true;
            else if (arg == "--numeric-sort") opt.globalFlags.numeric = true;
            else if (arg == "--random-sort") opt.globalFlags.randomSort = true;
            else if (arg == "--reverse") opt.globalFlags.reverse = true;
            else if (arg == "--version-sort") opt.globalFlags.version = true;
            else if (arg == "--unique") opt.unique = true;
            else if (arg == "--stable") opt.stable = true;
            else if (arg == "--zero-terminated") opt.lineTerminator = '\0';
            else if (arg == "--check" || arg == "--check=diagnose-first") opt.checkOnly = true;
            else if (arg == "--check=quiet" || arg == "--check=silent") { opt.checkOnly = true; opt.checkSilent = true; }
            else if (arg.rfind("--output=", 0) == 0) opt.outputFile = arg.substr(9);
            else if (arg.rfind("--field-separator=", 0) == 0) opt.delimiter = arg.substr(18)[0];
            else if (arg.rfind("--key=", 0) == 0) {
                KeyDefinition ks;
                KeyDefinition::parse(arg.substr(6), ks, opt.globalFlags);
                opt.keys.push_back(ks);
            }
            continue;
        }

        if (arg.size() > 1 && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                char c = arg[j];
                switch (c) {
                    case 'b': opt.globalFlags.ignoreLeadingBlanks = true; break;
                    case 'd': opt.globalFlags.dictOrder = true; break;
                    case 'f': opt.globalFlags.ignoreCase = true; break;
                    case 'g': opt.globalFlags.generalNumeric = true; break;
                    case 'h': opt.globalFlags.humanNumeric = true; break;
                    case 'i': opt.globalFlags.ignoreNonPrint = true; break;
                    case 'M': opt.globalFlags.month = true; break;
                    case 'n': opt.globalFlags.numeric = true; break;
                    case 'R': opt.globalFlags.randomSort = true; break;
                    case 'r': opt.globalFlags.reverse = true; break;
                    case 'V': opt.globalFlags.version = true; break;
                    case 'u': opt.unique = true; break;
                    case 's': opt.stable = true; break;
                    case 'z': opt.lineTerminator = '\0'; break;
                    case 'c': opt.checkOnly = true; break;
                    case 'C': opt.checkOnly = true; opt.checkSilent = true; break;
                    case 'o':
                        if (j + 1 < arg.size()) opt.outputFile = arg.substr(j + 1);
                        else if (++i < argc) opt.outputFile = argv[i];
                        j = arg.size();
                        break;
                    case 't':
                        if (j + 1 < arg.size()) opt.delimiter = arg[j + 1];
                        else if (++i < argc) opt.delimiter = argv[i][0];
                        j = arg.size();
                        break;
                    case 'k': {
                        std::string karg;
                        if (j + 1 < arg.size()) karg = arg.substr(j + 1);
                        else if (++i < argc) karg = argv[i];
                        KeyDefinition ks;
                        KeyDefinition::parse(karg, ks, opt.globalFlags);
                        opt.keys.push_back(ks);
                        j = arg.size();
                        break;
                    }
                    default:
                        std::cerr << "sequence: invalid option -- '" << c << "'\n";
                        std::cerr << "Try 'sequence --help' for more information.\n";
                        return false;
                }
            }
            continue;
        }

        opt.inputFiles.push_back(arg);
    }

    if (opt.inputFiles.empty()) {
        opt.inputFiles.push_back("-");
    }

    return true;
}
