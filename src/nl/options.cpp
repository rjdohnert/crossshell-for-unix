#include "options.hpp"

NlStyle NlStyle::parse(const std::string& arg) {
    NlStyle s;
    if (arg.empty()) return s;

    char c = arg[0];
    if (c == 'a') {
        s.mode = StyleMode::ALL;
    } else if (c == 't') {
        s.mode = StyleMode::NON_EMPTY;
    } else if (c == 'n') {
        s.mode = StyleMode::NONE;
    } else if (c == 'p') {
        s.mode = StyleMode::REGEX;
        s.rawPattern = arg.substr(1);
        try {
            s.pattern = std::regex(s.rawPattern);
        } catch (const std::regex_error& e) {
            std::cerr << "nl: invalid regular expression '" << s.rawPattern << "': " << e.what() << "\n";
            std::exit(1);
        }
    } else {
        std::cerr << "nl: unknown style type: " << arg << "\n";
        std::exit(1);
    }
    return s;
}

void NlOptions::printHelp() {
    std::cout << "Usage: nl [OPTION]... [FILE]...\n"
              << "Write each FILE to standard output, with line numbers added.\n"
              << "With no FILE, or when FILE is -, read standard input.\n\n"
              << "Options:\n"
              << "  -b, --body-numbering=STYLE   use STYLE for numbering body lines\n"
              << "  -d, --section-delimiter=CC   use CC for logical page delimiters\n"
              << "  -f, --footer-numbering=STYLE use STYLE for numbering footer lines\n"
              << "  -h, --header-numbering=STYLE use STYLE for numbering header lines\n"
              << "  -i, --line-increment=NUMBER  line number increment at each line\n"
              << "  -l, --join-blank-lines=NUMBER group of NUMBER empty lines counted as one\n"
              << "  -n, --number-format=FORMAT   insert line numbers according to FORMAT\n"
              << "  -p, --no-renumber            do not reset line numbers for each section\n"
              << "  -s, --number-separator=STRING add STRING after line number\n"
              << "  -v, --starting-line-number=NUMBER first line number for each section\n"
              << "  -w, --number-width=NUMBER    use NUMBER columns for line numbers\n"
              << "      --help                   display this help and exit\n"
              << "      --version                output version information and exit\n\n"
              << "FORMAT is one of: ln (left justified), rn (right justified), rz (right justified with leading zeroes).\n"
              << "STYLE is one of: a (all lines), t (non-empty lines only), n (no lines), pBRE (only lines matching BRE).\n";
}

void NlOptions::printVersion() {
    std::cout << "nl 1.0\n";
}

bool NlOptions::parse(int argc, char* argv[], NlOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printHelp();
            std::exit(0);
        } else if (arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-p" || arg == "--no-renumber") {
            opts.renumberPerPage = false;
        } else if (arg.rfind("-b", 0) == 0) {
            opts.bodyStyle = NlStyle::parse(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ""));
        } else if (arg.rfind("-h", 0) == 0 && arg != "-h") {
            opts.headerStyle = NlStyle::parse(arg.substr(2));
        } else if (arg == "-h") {
            if (i + 1 < argc && argv[i + 1][0] != '-') opts.headerStyle = NlStyle::parse(argv[++i]);
            else { printHelp(); std::exit(0); }
        } else if (arg.rfind("-f", 0) == 0) {
            opts.footerStyle = NlStyle::parse(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ""));
        } else if (arg.rfind("-d", 0) == 0) {
            std::string d = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            opts.delim = (d.size() == 1) ? d + ":" : d;
        } else if (arg.rfind("-v", 0) == 0) {
            opts.startNum = std::stoll(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
        } else if (arg.rfind("-i", 0) == 0) {
            opts.increment = std::stoll(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
        } else if (arg.rfind("-l", 0) == 0) {
            opts.blankLinesLimit = std::stoi(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
        } else if (arg.rfind("-s", 0) == 0) {
            opts.separator = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "\t");
        } else if (arg.rfind("-w", 0) == 0) {
            opts.width = std::stoi(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "6"));
        } else if (arg.rfind("-n", 0) == 0) {
            std::string fmt = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "rn");
            if (fmt == "ln") opts.format = NumberFormat::LN;
            else if (fmt == "rn") opts.format = NumberFormat::RN;
            else if (fmt == "rz") opts.format = NumberFormat::RZ;
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::cerr << "nl: unrecognized option '" << arg << "'\n";
            return false;
        } else {
            opts.files.push_back(arg);
        }
    }
    if (opts.files.empty()) opts.files.push_back("-");
    return true;
}
