#include "pinky_app.hpp"

int PinkyApplication::Run(int argc, wchar_t* argv[]) {
    PinkyOptions opts;
    if (!opts.Parse(argc, argv)) {
        opts.PrintUsage(argc > 0 ? argv[0] : L"pinky");
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage(argc > 0 ? argv[0] : L"pinky");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    if (opts.targets.empty() || (opts.forceShort && opts.targets.empty())) {
        auto sessions = PinkyProfileService::QueryActiveSessions();
        PinkyReporter::PrintSummary(sessions);
        return 0;
    }

    for (const auto& target : opts.targets) {
        size_t atPos = target.find(L'@');
        if (atPos != std::wstring::npos) {
            std::wstring uW = target.substr(0, atPos);
            std::wstring hW = target.substr(atPos + 1);

            std::string userA = PinkyProfileService::WideToUtf8(uW);
            std::string hostA = PinkyProfileService::WideToUtf8(hW);

            std::wcout << L"[" << hW << L"]\n";
            FingerClient::QueryRemote(userA, hostA);
        } else {
            if (opts.forceShort) {
                auto sessions = PinkyProfileService::QueryActiveSessions();
                PinkyReporter::PrintSummary(sessions);
            } else {
                PinkyUserProfile profile = PinkyProfileService::QueryUserProfile(target, opts.printPlan);
                PinkyReporter::PrintProfile(profile, opts.printPlan);
            }
        }
    }

    return 0;
}
