#include "sort_pipeline.hpp"

SequenceEngine::SequenceEngine(SequenceOptions opts) : options(std::move(opts)) {}

int SequenceEngine::execute() {
    #ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    #endif

    std::vector<std::string> lines;
    for (const auto& file : options.inputFiles) {
        std::istream* stream = &std::cin;
        std::ifstream fileStream;
        if (file != "-") {
            fileStream.open(file, std::ios::binary);
            if (!fileStream.is_open()) {
                std::cerr << "sequence: open failed: " << file << ": No such file or directory\n";
                return 2;
            }
            stream = &fileStream;
        }

        std::string line;
        while (std::getline(*stream, line, options.lineTerminator)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
        }
    }

    // Check-only mode (-c / -C)
    if (options.checkOnly) {
        for (size_t i = 1; i < lines.size(); ++i) {
            bool disorder = options.unique ? !KeyComparator::lineLess(lines[i - 1], lines[i], options)
                                           : KeyComparator::lineLess(lines[i], lines[i - 1], options);
            if (disorder) {
                if (!options.checkSilent) {
                    std::cerr << "sequence: disorder on line " << (i + 1) << ": " << lines[i] << "\n";
                }
                return 1;
            }
        }
        return 0;
    }

    // Random sort or stable sort
    if (options.globalFlags.randomSort) {
        std::mt19937 g(1337);
        std::shuffle(lines.begin(), lines.end(), g);
    } else {
        std::stable_sort(lines.begin(), lines.end(), [&](const std::string& a, const std::string& b) {
            return KeyComparator::lineLess(a, b, options);
        });
    }

    // Output stream
    std::ostream* out = &std::cout;
    std::ofstream outFileStream;
    if (!options.outputFile.empty()) {
        outFileStream.open(options.outputFile, std::ios::binary);
        if (!outFileStream.is_open()) {
            std::cerr << "sequence: open failed: " << options.outputFile << "\n";
            return 2;
        }
        out = &outFileStream;
    }

    // Write output (with optional -u unique filter)
    for (size_t i = 0; i < lines.size(); ++i) {
        if (options.unique && i > 0 && KeyComparator::lineEqual(lines[i - 1], lines[i], options)) {
            continue;
        }
        *out << lines[i] << options.lineTerminator;
    }

    return 0;
}
