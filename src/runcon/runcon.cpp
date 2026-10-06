#include "runcon.hpp"
#include "runcon_options.hpp"
#include "token_privilege.hpp"
#include "process_launcher.hpp"

class RunconApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        RunconOptions options;
        if (!RunconOptions::parse(argc, argv, options)) {
            return 1;
        }

        TokenPrivilegeGuard::enablePrivilege(L"SeAssignPrimaryTokenPrivilege");
        TokenPrivilegeGuard::enablePrivilege(L"SeIncreaseQuotaPrivilege");
        TokenPrivilegeGuard::enablePrivilege(L"SeSecurityPrivilege");

        std::wstring stringSid = IntegrityLevelMapper::mapLevelToSid(options.level);
        if (stringSid.empty()) {
            std::wcerr << L"runcon: unrecognized or invalid security level: " << options.level << L"\n";
            return 1;
        }

        return IntegrityProcessLauncher::launch(stringSid, options.commandArgs);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return RunconApp::run(argc, argv);
}
