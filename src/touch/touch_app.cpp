#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "time_parser.hpp"
#include "touch_app.hpp"
#include "touch_options.hpp"
#include "touch_pipe_buffer.hpp"

int TouchApplication::Run(int argc, wchar_t* argv[]) {
        if (argc <= 1) {
            OptionParser::PrintUsage();
            return 1;
        }

        TouchOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            if (argc > 1) {
                std::wstring firstArg = argv[1];
                if (firstArg == L"-h" || firstArg == L"--help" || firstArg == L"/?" || firstArg == L"-?" ||
                    firstArg == L"-v" || firstArg == L"--version") {
                    return 0;
                }
            }
            return 1;
        }

        FILETIME target_at, target_mt;
        if (!opts.ref_file.empty()) {
            if (!TimeParser::GetReferenceTimes(opts.ref_file, target_at, target_mt)) {
                std::wcerr << L"touch: " << opts.ref_file << L": reference file could not be read\n";
                return 1;
            }
        } else if (!opts.time_str.empty()) {
            if (!TimeParser::ParseTimeSpec(opts.time_str, target_at)) {
                std::wcerr << L"touch: out of range or bad time specification: " << opts.time_str << std::endl;
                return 1;
            }
            target_mt = target_at;
        } else if (!opts.date_str.empty()) {
            if (!TimeParser::ParseTimeSpec(opts.date_str, target_at)) {
                std::wcerr << L"touch: out of range or bad time specification: " << opts.date_str << std::endl;
                return 1;
            }
            target_mt = target_at;
        } else {
            SYSTEMTIME st;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &target_at);
            target_mt = target_at;
        }

        FILE* outputPipe = opts.pipe_command.empty() ? nullptr : _wpopen(opts.pipe_command.c_str(), L"w");
        if (!opts.pipe_command.empty() && !outputPipe) return 1;

        std::wstreambuf* oldOutput = nullptr;
        TouchPipeBuffer* pipeBuffer = nullptr;
        if (outputPipe) {
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new TouchPipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        OutputFormatter::EmitHeader(opts.output_format, std::wcout);

        bool overall_success = true;
        for (const auto& target : opts.targets) {
            bool touched = m_toucher.Touch(target, opts, target_at, target_mt);
            if (!touched) {
                overall_success = false;
            } else {
                OutputFormatter::EmitRecord(target, L"success", opts.output_format, std::wcout);
            }
        }

        if (pipeBuffer) {
            std::wcout.flush();
            std::wcout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(outputPipe);
        }

        return overall_success ? 0 : 1;
    }
