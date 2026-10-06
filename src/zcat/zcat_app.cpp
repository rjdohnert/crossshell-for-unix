#include "option_parser.hpp"
#include "pipe_buffer.hpp"
#include "zcat_app.hpp"
#include "zcat_options.hpp"

ZcatApplication::ZcatApplication() : m_streamer(m_runner) {}

int ZcatApplication::Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"zcat";
        if (argc <= 1) {
            OptionParser::PrintUsage(progName);
            return 1;
        }

        ZcatOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 2;
        }

        if (opts.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }
        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        FILE* pipe = nullptr;
        std::streambuf* oldOutput = nullptr;
        PipeBuffer* pipeBuffer = nullptr;
        if (!opts.pipeCommand.empty()) {
            pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (pipe) {
                oldOutput = std::cout.rdbuf();
                pipeBuffer = new PipeBuffer(pipe);
                std::cout.rdbuf(pipeBuffer);
            }
        }

        int overall = 0;
        for (const std::wstring& f : opts.files) {
            fs::path inPath(f);
            std::error_code ec;
            if (!fs::exists(inPath, ec) || fs::is_directory(inPath, ec)) {
                std::wcerr << L"zcat: cannot access: " << f << L"\n";
                overall = 1;
                continue;
            }

            int rc = m_streamer.EmitGzipFile(inPath, opts.format);
            if (rc != 0) {
                std::wcerr << L"zcat: failed processing: " << f << L"\n";
                overall = 1;
            }
        }

        if (pipeBuffer) {
            std::cout.flush();
            std::cout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(pipe);
        }

        return overall;
    }
