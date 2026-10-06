#include "nfsctl_app.hpp"

int NfsctlApplication::Run(int argc, wchar_t* argv[]) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    WinsockSubsystem winsock;
    if (!winsock.Ok()) {
        DWORD err = static_cast<DWORD>(winsock.Status());
        NfsOutputFormatter::PrintWin32Error(L"WSAStartup failed", err);
        return NfsOutputFormatter::NormalizeExitCode(err);
    }

    if (argc < 2) {
        NfsctlHelpSystem::PrintMainHelp();
        return ERROR_SUCCESS;
    }

    std::wstring main_switch = argv[1];
    std::vector<std::wstring> sub_args;
    for (int i = 2; i < argc; ++i) {
        sub_args.push_back(argv[i]);
    }

    GlobalOptions options;
    DWORD parseErr = NfsctlOptionsParser::ParseGlobalOptions(sub_args, options);
    if (parseErr != ERROR_SUCCESS) {
        return options.win32Exit ? static_cast<int>(parseErr) : NfsOutputFormatter::NormalizeExitCode(parseErr);
    }

    if ((options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) &&
        (main_switch == L"--mount" || main_switch == L"--umount" ||
         (main_switch == L"--nfsconf" && !NfsctlHelpSystem::HasHelpFlag(sub_args)) ||
         (main_switch == L"--rpcdebug" && !NfsctlHelpSystem::HasHelpFlag(sub_args)))) {
        fwprintf(stderr, L"Error: CSV/TSV output is only supported for read/query commands.\n");
        DWORD modeErr = ERROR_INVALID_PARAMETER;
        return options.win32Exit ? static_cast<int>(modeErr) : NfsOutputFormatter::NormalizeExitCode(modeErr);
    }

    if (main_switch == L"--help" || main_switch == L"-h") {
        NfsctlHelpSystem::PrintMainHelp();
        return ERROR_SUCCESS;
    }
    if (main_switch == L"--version" || main_switch == L"-v") {
        wprintf(L"NFSCTL v5.0.0\n");
        return ERROR_SUCCESS;
    }

    DWORD result = ERROR_INVALID_PARAMETER;

    if (main_switch == L"--mounts" || main_switch == L"-m" || 
        main_switch == L"--mount"  || main_switch == L"--umount") {
        std::vector<std::wstring> full_args;
        full_args.push_back(main_switch);
        full_args.insert(full_args.end(), sub_args.begin(), sub_args.end());
        if (NfsctlHelpSystem::HasHelpFlag(full_args)) {
            MountsToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        result = MountsToolController::Execute(full_args, options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }

    if (main_switch == L"--nfsstat" || main_switch == L"-st") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            NfsStatToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        result = NfsStatToolController::Execute(options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
    if (main_switch == L"--exportfs" || main_switch == L"-e") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            ExportFsToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        result = ExportFsToolController::Execute(options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
    if (main_switch == L"--showmount" || main_switch == L"-s") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            ShowMountToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        if (sub_args.empty()) {
            fwprintf(stderr, L"Error: Host argument missing. Example: nfsctl.exe --showmount 192.168.1.50\n");
            result = ERROR_INVALID_PARAMETER;
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        result = ShowMountToolController::Execute(sub_args[0].c_str(), options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
    if (main_switch == L"--nfsconf" || main_switch == L"-c") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            NfsConfToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        result = NfsConfToolController::Execute(sub_args, options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
    if (main_switch == L"--nfsidmap" || main_switch == L"-id") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            NfsIdMapToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        if (sub_args.empty()) {
            fwprintf(stderr, L"Error: Account name missing. Example: nfsctl.exe --nfsidmap Administrator\n");
            result = ERROR_INVALID_PARAMETER;
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        result = NfsIdMapToolController::Execute(sub_args[0].c_str(), options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
    if (main_switch == L"--rpcdebug" || main_switch == L"-d") {
        if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
            RpcDebugToolController::PrintHelp();
            return ERROR_SUCCESS;
        }
        result = RpcDebugToolController::Execute(sub_args, options);
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }

    fwprintf(stderr, L"Error: Unknown switch '%s'. Run 'nfsctl.exe --help'.\n", main_switch.c_str());
    result = ERROR_INVALID_PARAMETER;
    return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
}
