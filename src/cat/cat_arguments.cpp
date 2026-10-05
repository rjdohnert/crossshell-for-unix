#include "cat_options.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

bool ArgumentParser::parse(int argc, char* argv[], CatOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            HelpFormatter::printHelp();
            std::exit(0);
        }
        if (arg == "--json") {
            options.outputJson = true;
            continue;
        }
        if (arg == "-") {
            options.files.push_back("-");
            continue;
        }
        if (arg.length() > 1 && arg[0] == '-') {
            for (size_t c = 1; c < arg.length(); ++c) {
                switch (arg[c]) {
                    case 'b': options.numberNonBlank = true; break;
                    case 'e': options.showEnds = true; break;
                    case 'n': options.numberLines = true; break;
                    case 'r': options.squeezeBlanks = true; break;
                    case 's': options.silent = true; break;
                    case 't': options.showTabs = true; break;
                    case 'u': options.unbuffered = true; break;
                    case 'v': options.showNonPrinting = true; break;
                    default:
                        std::cerr << "cat: illegal option -- " << arg[c] << "\n"
                                  << "usage: cat [-benrstuv] [-] [file ...]\n";
                        return false;
                }
            }
        } else {
            options.files.push_back(arg);
        }
    }
    if (options.files.empty()) {
        options.files.push_back("-");
    }
    options.normalizeDependencies();
    return true;
}
