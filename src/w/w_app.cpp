#include "pipe_buffer.hpp"
#include "pipe_session.hpp"
#include "session_collector.hpp"
#include "session_reporter.hpp"
#include "string_encoding.hpp"
#include "w_app.hpp"
#include "w_help.hpp"
#include "wts_deleter.hpp"

int runW(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");

    bool showHeader = true;
    bool shortFormat = false;
    bool showFrom = false;
    std::wstring targetUser = L"";
    OutputFormat outputFormat = OutputFormat::Human;
    std::string pipeCommand;

    // CLI Arguments Parser
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-?" || arg == L"--help") {
            PrintHelp();
            return 0;
        } else if (arg == L"-V" || arg == L"--version") {
            PrintVersion();
            return 0;
        } else if (arg == L"-h" || arg == L"--no-header") {
            showHeader = false;
        } else if (arg == L"-s" || arg == L"--short") {
            shortFormat = true;
        } else if (arg == L"-f" || arg == L"--from" || arg == L"-i" || arg == L"--ip-addr") {
            showFrom = true;
        } else if (arg == L"-l" || arg == L"--long") {
            shortFormat = false;
        } else if (arg == L"--json" || arg == L"-j") {
            outputFormat = OutputFormat::Json;
        } else if (arg == L"--csv") {
            outputFormat = OutputFormat::Csv;
        } else if (arg == L"--tsv") {
            outputFormat = OutputFormat::Tsv;
        } else if (arg == L"--table") {
            outputFormat = OutputFormat::Table;
        } else if (arg == L"--output" && i + 1 < argc) {
            std::wstring fmt = argv[++i];
            if (fmt == L"json") outputFormat = OutputFormat::Json;
            else if (fmt == L"csv") outputFormat = OutputFormat::Csv;
            else if (fmt == L"tsv") outputFormat = OutputFormat::Tsv;
            else if (fmt == L"table") outputFormat = OutputFormat::Table;
        } else if (arg == L"--pipe" && i + 1 < argc) {
            std::wstring commandText = argv[++i];
            pipeCommand = WStrToStr(commandText);
        } else if (!arg.empty() && arg[0] != L'-') {
            targetUser = arg;
        }
    }

    PWTS_SESSION_INFOW pSessions = nullptr;
    DWORD sessionCount = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &sessionCount)) {
        std::cerr << "Error: Unable to enumerate terminal sessions (Error Code: " << GetLastError() << ")\n";
        return 1;
    }

    std::unique_ptr<WTS_SESSION_INFOW, WtsDeleter> spSessions(pSessions);
    PipeSession pipeSession(pipeCommand);

    const auto userSessions = collectUserSessions(pSessions, sessionCount, targetUser);
    return reportUserSessions(userSessions, showHeader, shortFormat, showFrom, outputFormat);
}
