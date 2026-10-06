#include "options.hpp"

void PurgeOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // 1. POSIX Double-Dash Options (--keep=2, --log, etc.)
        if (arg.rfind("--", 0) == 0) {
            std::string opt = StringHelper::ToUpper(arg.substr(2));
            if (opt.rfind("KEEP=", 0) == 0) {
                keepCount = std::max(1, std::stoi(opt.substr(5)));
            } else if (opt == "LOG") {
                log = true;
            } else if (opt == "CONFIRM") {
                confirm = true;
            } else if (opt == "ERASE") {
                erase = true;
            } else if (opt == "GRAND-TOTAL" || opt == "GRAND_TOTAL") {
                grandTotal = true;
            } else if (opt.rfind("EXCLUDE=", 0) == 0) {
                excludePattern = StringHelper::StringToWString(arg.substr(10));
            } else if (opt.rfind("OUTPUT=", 0) == 0) {
                outputSpec = StringHelper::StringToWString(arg.substr(9));
            } else if (opt == "HELP") {
                help = true;
                if (i + 1 < argc && argv[i + 1][0] != '-' && argv[i + 1][0] != '/') {
                    helpTopic = StringHelper::ToUpper(argv[++i]);
                }
            } else {
                std::cerr << "%DCL-W-IVQUAL, unrecognized switch --" << opt << "\n";
            }
        }
        // 2. VMS & CMD Qualifiers (/KEEP=2, /LOG, /CONFIRM, /?)
        else if (arg[0] == '/') {
            std::string q = StringHelper::ToUpper(arg.substr(1));
            if (q.rfind("KEEP=", 0) == 0) {
                keepCount = std::max(1, std::stoi(q.substr(5)));
            } else if (q.rfind("K:", 0) == 0) {
                keepCount = std::max(1, std::stoi(q.substr(2)));
            } else if (q == "LOG" || q == "L") {
                log = true;
            } else if (q == "CONFIRM" || q == "C") {
                confirm = true;
            } else if (q == "ERASE" || q == "E") {
                erase = true;
            } else if (q == "GRAND_TOTAL" || q == "GRANDTOTAL" || q == "G") {
                grandTotal = true;
            } else if (q.rfind("EXCLUDE=", 0) == 0) {
                excludePattern = StringHelper::StringToWString(arg.substr(9));
            } else if (q.rfind("OUTPUT=", 0) == 0) {
                outputSpec = StringHelper::StringToWString(arg.substr(8));
            } else if (q == "HELP" || q == "?" || q == "H") {
                help = true;
                if (i + 1 < argc && argv[i + 1][0] != '/' && argv[i + 1][0] != '-') {
                    helpTopic = StringHelper::ToUpper(argv[++i]);
                }
            } else {
                std::cerr << "%DCL-W-IVQUAL, unrecognized qualifier /" << q << "\n";
            }
        }
        // 3. POSIX Short Options (-k 2, -l, -c, -e, -g, -x pat)
        else if (arg[0] == '-') {
            std::string s = arg.substr(1);
            if (s == "k" && i + 1 < argc) {
                keepCount = std::max(1, std::stoi(argv[++i]));
            } else if (s == "l") {
                log = true;
            } else if (s == "c") {
                confirm = true;
            } else if (s == "e") {
                erase = true;
            } else if (s == "g") {
                grandTotal = true;
            } else if (s == "x" && i + 1 < argc) {
                excludePattern = StringHelper::StringToWString(argv[++i]);
            } else if (s == "o" && i + 1 < argc) {
                outputSpec = StringHelper::StringToWString(argv[++i]);
            } else if (s == "h" || s == "?") {
                help = true;
                if (i + 1 < argc && argv[i + 1][0] != '-' && argv[i + 1][0] != '/') {
                    helpTopic = StringHelper::ToUpper(argv[++i]);
                }
            } else {
                std::cerr << "%DCL-W-IVQUAL, unrecognized option -" << s << "\n";
            }
        }
        // 4. Positional Argument (Filespec or HELP verb)
        else {
            std::string p = StringHelper::ToUpper(arg);
            if (p == "HELP") {
                help = true;
                if (i + 1 < argc) {
                    helpTopic = StringHelper::ToUpper(argv[++i]);
                }
            } else {
                fileSpec = StringHelper::StringToWString(arg);
            }
        }
    }
}
