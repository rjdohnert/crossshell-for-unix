#include "user_session.hpp"
#include "who_reporter.hpp"

void WhoReporter::PrintHeader() {
        std::wcout << std::left 
                  << std::setw(18) << L"NAME"
                  << std::setw(14) << L"LINE"
                  << std::setw(18) << L"TIME"
                  << L"COMMENT\n";
    }

void WhoReporter::PrintSession(const UserSession& s) {
        std::wstring userDisplay = s.domain.empty() ? s.username : (s.domain + L"\\" + s.username);
        std::wstring comment = L"(" + s.clientName + L")";

        std::wcout << std::left 
                  << std::setw(18) << userDisplay
                  << std::setw(14) << s.line
                  << std::setw(18) << s.logonTime
                  << comment << L"\n";
    }

void WhoReporter::PrintCount(const std::vector<UserSession>& sessions) {
        for (size_t i = 0; i < sessions.size(); ++i) {
            std::wcout << sessions[i].username << (i + 1 < sessions.size() ? L" " : L"");
        }
        std::wcout << L"\n# users=" << sessions.size() << L"\n";
    }
