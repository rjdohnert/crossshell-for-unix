#include "file_tailer.hpp"
#include "follow_controller.hpp"
#include "stream_tailer.hpp"
#include "tail_app.hpp"
#include "tail_options.hpp"
#include "tail_output.hpp"

int TailApplication::Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        TailOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        bool show_headers = (opts.files.size() > 1 || opts.verbose) && !opts.quiet;
        bool first_header = true;

        for (const auto& file_path : opts.files) {
            if (file_path == L"-") {
                if (show_headers) TailOutput::PrintHeader(L"-", first_header);
                if (opts.count_lines) {
                    StreamTailer::TailLines(std::cin, opts.count, opts.from_start);
                } else {
                    StreamTailer::TailBytes(std::cin, opts.count, opts.from_start);
                }
                continue;
            }

            HANDLE hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hFile == INVALID_HANDLE_VALUE) {
                std::wcerr << L"tail: cannot open '" << file_path << L"' for reading: No such file or directory\n";
                continue;
            }

            if (show_headers) TailOutput::PrintHeader(file_path, first_header);

            if (opts.count_lines) {
                FileTailer::TailLines(hFile, opts.count, opts.from_start);
            } else {
                FileTailer::TailBytes(hFile, opts.count, opts.from_start);
            }

            CloseHandle(hFile);
        }

        if (opts.follow) {
            FollowController::FollowFiles(opts.files, opts, show_headers);
        }

        return 0;
    }
