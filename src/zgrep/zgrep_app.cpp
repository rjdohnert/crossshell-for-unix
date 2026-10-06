#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "zgrep_app.hpp"
#include "zgrep_options.hpp"

ZgrepApplication::ZgrepApplication() : m_decompressor(m_runner) {}

int ZgrepApplication::Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"zgrep";
        if (argc <= 1) {
            OptionParser::PrintUsage(progName);
            return 1;
        }

        ZgrepOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }
        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        wchar_t tempPathBuf[MAX_PATH] = {};
        DWORD tp = GetTempPathW(MAX_PATH, tempPathBuf);
        if (tp == 0 || tp >= MAX_PATH) {
            std::wcerr << L"zgrep: failed to get temporary path\n";
            return 1;
        }

        wchar_t tempNameBuf[MAX_PATH] = {};
        if (GetTempFileNameW(tempPathBuf, L"zgr", 0, tempNameBuf) == 0) {
            std::wcerr << L"zgrep: failed to create temporary directory\n";
            return 1;
        }
        fs::path tempRoot = tempNameBuf;
        std::error_code ec;
        fs::remove(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec) {
            std::wcerr << L"zgrep: failed to create temporary directory\n";
            return 1;
        }

        std::vector<fs::path> created;
        std::vector<std::wstring> grepArgs;
        grepArgs.reserve(opts.rawArgs.size());

        int overall = 0;
        int outIndex = 0;

        for (const auto& a : opts.rawArgs) {
            if (a == L"-") {
                std::wcerr << L"zgrep: stdin mode is not supported in this baseline build\n";
                overall = 1;
                continue;
            }

            if (!a.empty() && a[0] == L'-') {
                grepArgs.push_back(a);
                continue;
            }

            fs::path p = a;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
                std::wstring w = p.wstring();
                bool isGz = w.size() >= 3 && _wcsicmp(w.c_str() + (w.size() - 3), L".gz") == 0;
                if (isGz) {
                    fs::path out = tempRoot / (L"zgrep_" + std::to_wstring(outIndex++) + L".txt");
                    if (!m_decompressor.Decompress(p, out)) {
                        std::wcerr << L"zgrep: failed to decompress: " << p.wstring() << L"\n";
                        overall = 1;
                        continue;
                    }
                    created.push_back(out);
                    grepArgs.push_back(out.wstring());
                    continue;
                }
            }

            grepArgs.push_back(a);
        }

        if (grepArgs.empty()) {
            fs::remove_all(tempRoot, ec);
            return (overall == 0) ? 1 : overall;
        }

        std::wstring grepBackend = m_backendLocator.Resolve();
        std::vector<std::wstring> invoke;
        invoke.reserve(grepArgs.size() + 1);
        invoke.push_back(grepBackend);
        for (const auto& a : grepArgs) invoke.push_back(a);

        HANDLE capture = INVALID_HANDLE_VALUE;
        std::wstring capturePath;
        if (opts.format != OutputFormat::Human || !opts.pipeCommand.empty()) {
            wchar_t tempFile[MAX_PATH] = {};
            GetTempFileNameW(tempPathBuf, L"zgo", 0, tempFile);
            capturePath = tempFile;
            capture = CreateFileW(capturePath.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (capture != INVALID_HANDLE_VALUE) SetHandleInformation(capture, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }

        int grepExit = m_runner.Run(grepBackend.c_str(), invoke, capture);

        if (capture != INVALID_HANDLE_VALUE) {
            CloseHandle(capture);
            std::ifstream input(capturePath);
            FILE* pipe = opts.pipeCommand.empty() ? nullptr : _wpopen(opts.pipeCommand.c_str(), L"w");
            OutputFormatter::EmitRecords(input, opts.format, pipe);
            input.close();
            DeleteFileW(capturePath.c_str());
            if (pipe) _pclose(pipe);
        }

        fs::remove_all(tempRoot, ec);
        return (overall != 0) ? overall : grepExit;
    }
