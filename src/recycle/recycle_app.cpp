#include "console_prompter.hpp"
#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "recycle_app.hpp"
#include "recycle_engine.hpp"
#include "recycle_options.hpp"
#include "wide_pipe_buffer.hpp"

int RecycleApplication::Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        RecycleOptions opts = m_parser.Parse(argc, argv);

        FILE* outputPipe = nullptr;
        WidePipeBuffer* pipeBuffer = nullptr;
        std::wstreambuf* oldOutput = nullptr;

        if (!opts.pipeCommand.empty()) {
            outputPipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (!outputPipe) {
                std::wcerr << L"recycle: failed to start pipe command\n";
                return 2;
            }
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new WidePipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        auto cleanupPipe = [&]() {
            if (pipeBuffer) {
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                pipeBuffer = nullptr;
                _pclose(outputPipe);
                outputPipe = nullptr;
            }
        };

        if (opts.showHelp) {
            cleanupPipe();
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            cleanupPipe();
            OptionParser::PrintVersion();
            return 0;
        }

        if (opts.targets.empty()) {
            cleanupPipe();
            std::wcerr << L"recycle: error: missing operand\n";
            std::wcerr << L"Try 'recycle --help' for more information.\n";
            return 2;
        }

        OutputFormatter::EmitHeader(opts.outputFormat);

        int successCount = 0;
        int failureCount = 0;

        for (const auto& target : opts.targets) {
            fs::path p(target);

            if (opts.interactive) {
                if (!ConsolePrompter::PromptUser(p)) {
                    if (opts.verbose) {
                        std::wcout << L"[SKIPPED] " << p.wstring() << L"\n";
                    }
                    continue;
                }
            }

            if (opts.verbose) {
                std::wcout << L"[RECYCLING] " << fs::absolute(p).wstring() << L" ... ";
            }

            bool ok = RecycleEngine::RecycleItem(p, opts.quiet);
            if (ok) {
                successCount++;
            } else {
                failureCount++;
            }

            OutputFormatter::EmitItem(opts.outputFormat, p.wstring(), ok, opts.verbose, opts.quiet);
        }

        OutputFormatter::EmitSummary(successCount, failureCount, opts.quiet, opts.outputFormat, opts.targets.size(), opts.verbose);

        int exitCode = (failureCount == 0) ? 0 : 1;
        cleanupPipe();
        return exitCode;
    }
