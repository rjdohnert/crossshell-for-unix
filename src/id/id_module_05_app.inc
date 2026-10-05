// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class IdApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        NativePipeConfigurator::Configure();

        IdOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"id");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"id");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        UserTokenData data = opts.targetUser.empty() ? IdentityEngine::GetCurrentProcessData()
                                                    : IdentityEngine::GetUserDataByName(opts.targetUser);

        if (data.user.sidString.empty()) {
            std::wcerr << L"id: '" << (opts.targetUser.empty() ? L"current user" : opts.targetUser) << L"': no such user\n";
            return 1;
        }

        IdReporter::Emit(opts, data);
        return 0;
    }
};
