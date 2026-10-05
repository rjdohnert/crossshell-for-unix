#include "cat_engine.hpp"

#include <fstream>
#include <iostream>

CatEngine::CatEngine(const CatOptions& opts) : options(opts), processor(options) {}

int CatEngine::execute() {
    int exitCode = 0;
    for (const auto& filename : options.files) {
        if (filename == "-") {
            if (options.requiresFormatting()) {
                processor.processFormatted(std::cin);
            } else {
                processor.processRaw(std::cin);
            }
            continue;
        }

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            if (!options.silent) {
                std::cerr << "cat: cannot open " << filename << ": No such file or directory\n";
            }
            exitCode = 1;
            continue;
        }
        if (options.requiresFormatting()) {
            processor.processFormatted(file);
        } else {
            processor.processRaw(file);
        }
        file.close();
    }
    return exitCode;
}
