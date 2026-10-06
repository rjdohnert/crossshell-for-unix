#include "job_limit_manager.hpp"
#include "privilege_escalator.hpp"
#include "ulimit_app.hpp"
#include "ulimit_options.hpp"
#include "ulimit_reporter.hpp"
#include "wide_pipe_buffer.hpp"

int UlimitApplication::Run(int argc, wchar_t* argv[]) const {
        UlimitOptions options;
        options.Parse(argc, argv);

        if (options.show_version) {
            UlimitReporter::PrintVersion();
            return 0;
        }

        if (options.show_help) {
            UlimitReporter::PrintHelp();
            return 0;
        }

        if (options.parse_error) {
            return 1;
        }

        FILE* outputPipe = nullptr;
        WidePipeBuffer* pipeBuffer = nullptr;
        std::wstreambuf* oldOutput = nullptr;
        if (!options.pipe_command.empty()) {
            outputPipe = _wpopen(options.pipe_command.c_str(), L"w");
            if (!outputPipe) return 1;
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new WidePipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        if (options.show_all || options.command.empty()) {
            UlimitReporter::PrintSummary(options.output_format);
            if (options.command.empty()) {
                if (pipeBuffer) {
                    std::wcout.flush();
                    std::wcout.rdbuf(oldOutput);
                    delete pipeBuffer;
                    _pclose(outputPipe);
                }
                return 0;
            }
        }

        PrivilegeEscalator::EnableDebugPrivilege();

        if (!options.command.empty()) {
            int result = JobLimitManager::RunChildUnderLimits(options) ? 0 : 1;
            if (pipeBuffer) {
                std::wcout.flush();
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                _pclose(outputPipe);
            }
            return result;
        }

        return 0;
    }
