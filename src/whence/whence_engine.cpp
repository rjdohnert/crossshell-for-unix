#include "command_classifier.hpp"
#include "whence_engine.hpp"
#include "whence_options.hpp"
#include "whence_reporter.hpp"
#include "whence_result.hpp"

WhenceEngine::WhenceEngine(WhenceOptions opts) : options(std::move(opts)) {}

int WhenceEngine::execute() {
        std::vector<WhenceResult> results;
        bool allFound = true;

        for (const auto& target : options.targets) {
            bool found = false;

            if (!options.pathSearchOnly && CommandClassifier::isBuiltin(target)) {
                results.push_back({ target, L"builtin", L"" });
                found = true;
                if (!options.showAll) continue;
            }

            std::vector<std::wstring> paths = CommandClassifier::findInPath(target, options.showAll);
            for (const auto& p : paths) {
                results.push_back({ target, L"executable", p });
                found = true;
            }

            if (!found) {
                results.push_back({ target, L"not_found", L"" });
                allFound = false;
            }
        }

        WhenceReporter::dispatch(results, options.outputFormat, options.verbose, options.pipeCommand);
        return allFound ? 0 : 1;
    }
