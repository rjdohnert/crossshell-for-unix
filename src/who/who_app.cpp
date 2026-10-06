#include "session_enumerator.hpp"
#include "user_session.hpp"
#include "who_app.hpp"
#include "who_options.hpp"
#include "who_reporter.hpp"

int WhoApplication::Run(int argc, wchar_t* argv[]) {
        WhoOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"who");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"who");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.optBoot) {
            std::wcout << L"system boot  " << SessionEnumerator::GetBootTime() << L"\n";
            return 0;
        }

        auto sessions = SessionEnumerator::GetLoggedOnUsers();

        if (opts.optAmI) {
            std::wstring currentUser = SessionEnumerator::GetCurrentUserName();
            if (opts.optHeader) WhoReporter::PrintHeader();
            for (const auto& s : sessions) {
                if (_wcsicmp(s.username.c_str(), currentUser.c_str()) == 0) {
                    WhoReporter::PrintSession(s);
                    return 0;
                }
            }
            UserSession current;
            current.username = currentUser;
            current.line = L"console";
            current.logonTime = L"-";
            current.clientName = L"local";
            WhoReporter::PrintSession(current);
            return 0;
        }

        if (opts.optCount) {
            WhoReporter::PrintCount(sessions);
            return 0;
        }

        if (opts.optHeader) WhoReporter::PrintHeader();

        for (const auto& s : sessions) {
            WhoReporter::PrintSession(s);
        }

        return 0;
    }
