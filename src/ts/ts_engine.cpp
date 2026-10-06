#include "structured_reporter.hpp"
#include "ts_engine.hpp"
#include "ts_options.hpp"

TsEngine::TsEngine(TsOptions opts)
        : options(opts), formatter(std::move(opts)) {}

int TsEngine::execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        std::vector<std::pair<std::string, std::string>> structuredRecords;
        std::string line;

        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::string prefix = formatter.generatePrefix();

            if (options.outputFormat == 0 && options.pipeCommand.empty()) {
                std::cout << prefix << " " << line << "\n";
                if (options.flushEveryLine) {
                    std::cout.flush();
                }
            } else {
                structuredRecords.emplace_back(prefix, line);
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            return StructuredReporter::output(structuredRecords, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
