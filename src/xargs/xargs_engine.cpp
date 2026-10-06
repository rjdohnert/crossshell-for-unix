#include "command_line_escaper.hpp"
#include "input_tokenizer.hpp"
#include "process_batch_runner.hpp"
#include "xargs_engine.hpp"
#include "xargs_options.hpp"

XargsEngine::XargsEngine(XargsOptions opts) : options(std::move(opts)) {}

int XargsEngine::execute() {
        std::vector<std::string> tokens = InputTokenizer::readAll(std::cin, options);

        if (tokens.empty()) {
            if (options.noRunIfEmpty) return 0;
            return ProcessBatchRunner::runCommand(options.command, options.verbose);
        }

        if (!options.replaceStr.empty()) {
            for (const auto& tok : tokens) {
                std::vector<std::string> replaced = options.command;
                for (auto& arg : replaced) {
                    arg = CommandLineEscaper::replaceAll(arg, options.replaceStr, tok);
                }
                int code = ProcessBatchRunner::runCommand(replaced, options.verbose);
                if (code != 0) return code;
            }
            return 0;
        }

        size_t batchSize = (options.maxArgs > 0) ? static_cast<size_t>(options.maxArgs)
                         : (options.maxLines > 0) ? static_cast<size_t>(options.maxLines)
                         : tokens.size();

        for (size_t i = 0; i < tokens.size(); i += batchSize) {
            std::vector<std::string> currentCmd = options.command;
            for (size_t j = i; j < (std::min)(tokens.size(), i + batchSize); ++j) {
                currentCmd.push_back(tokens[j]);
            }

            int code = ProcessBatchRunner::runCommand(currentCmd, options.verbose);
            if (code != 0) return code;
        }

        return 0;
    }
