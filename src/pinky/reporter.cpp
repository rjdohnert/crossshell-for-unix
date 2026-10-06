#include "reporter.hpp"
#include "engine.hpp"

void PinkyReporter::PrintSummary(const std::vector<PinkySessionSummary>& sessions) {
    std::wcout << std::left
              << std::setw(14) << L"Login"
              << std::setw(22) << L"Name"
              << std::setw(12) << L"Tty"
              << std::setw(8)  << L"Idle"
              << std::setw(14) << L"Login Time"
              << L"Office/Host\n";

    for (const auto& s : sessions) {
        std::wcout << std::left
                  << std::setw(14) << s.username
                  << std::setw(22) << s.fullName
                  << std::setw(12) << s.line
                  << std::setw(8)  << s.idle
                  << std::setw(14) << s.logonTime
                  << L"(" << s.host << L")\n";
    }
}

void PinkyReporter::PrintProfile(const PinkyUserProfile& profile, bool printPlan) {
    if (!profile.userFound) {
        std::wcerr << L"pinky: " << profile.username << L": no such user.\n";
        return;
    }

    std::wcout << L"Login: " << std::left << std::setw(25) << profile.username 
              << L"Name: " << profile.fullName << L"\n";
    std::wcout << L"Directory: " << std::left << std::setw(21) << profile.homeDir 
              << L"Shell: " << profile.shell << L"\n";
    std::wcout << L"Comment: " << profile.comment << L"\n";

    if (profile.lastLogon > 0) {
        std::wcout << L"Last logon: " << PinkyProfileService::FormatTime(profile.lastLogon) << L"\n";
    } else {
        std::wcout << L"Never logged in.\n";
    }

    std::wcout << L"No Mail.\n";

    if (printPlan) {
        if (!profile.planLines.empty()) {
            std::wcout << L"Plan:\n";
            for (const auto& line : profile.planLines) {
                std::wcout << line << L"\n";
            }
        } else {
            std::wcout << L"No Plan.\n";
        }
    }
}
