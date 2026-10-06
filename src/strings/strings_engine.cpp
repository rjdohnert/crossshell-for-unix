#include "strings_engine.hpp"
#include "strings_options.hpp"
#include "strings_reporter.hpp"

bool StringsEngine::isPrintable(unsigned char c) {
        return (c >= 32 && c <= 126) || c == '\t';
    }

void StringsEngine::processStream(std::istream& in, std::vector<std::string>& results) const {
        std::string current;
        char ch = 0;
        while (in.get(ch)) {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (isPrintable(uc)) {
                current.push_back(ch);
            } else {
                if (current.length() >= options.minLength) {
                    if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                        results.push_back(current);
                    } else {
                        std::cout << current << "\n";
                    }
                }
                current.clear();
            }
        }

        if (current.length() >= options.minLength) {
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                results.push_back(current);
            } else {
                std::cout << current << "\n";
            }
        }
    }

StringsEngine::StringsEngine(StringsOptions opts) : options(std::move(opts)) {}

int StringsEngine::execute() {
        std::vector<std::string> results;
        bool allOk = true;

        for (const auto& path : options.files) {
            if (path == "-") {
                processStream(std::cin, results);
            } else {
                std::ifstream in(path, std::ios::binary);
                if (!in.is_open()) {
                    std::cerr << "strings: '" << path << "': No such file\n";
                    allOk = false;
                    continue;
                }
                processStream(in, results);
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            StringsReporter::dispatch(results, options.outputFormat, options.pipeCommand);
        }

        return allOk ? 0 : 1;
    }
