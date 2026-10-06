#include "evaluator.hpp"
#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "test_app.hpp"
#include "test_options.hpp"

int TestApplication::Run(int argc, wchar_t* argv[]) const {
        TestOptions opts = m_parser.Parse(argc, argv);

        if (opts.tokens.size() == 1 && opts.tokens[0] == L"--help") {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.tokens.size() == 1 && opts.tokens[0] == L"--version") {
            OptionParser::PrintVersion();
            return 0;
        }

        if (opts.isBracket) {
            if (opts.tokens.empty() || opts.tokens.back() != L"]") {
                std::fwprintf(stderr, L"[: missing ']'\n");
                return 2;
            }
            opts.tokens.pop_back();
        }

        if (opts.tokens.empty()) {
            return 1;
        }

        try {
            Evaluator eval(opts.tokens);
            bool result = eval.Parse();
            OutputFormatter::Emit(opts.format, opts.pipeCommand, result);
            return result ? 0 : 1;
        } catch (const std::exception& ex) {
            std::fwprintf(stderr, L"test: %S\n", ex.what());
            return 2;
        }
    }
