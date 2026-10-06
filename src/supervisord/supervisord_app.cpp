#include "config_parser.hpp"
#include "ipc_client.hpp"
#include "ipc_command_parser.hpp"
#include "logger.hpp"
#include "path_encoding.hpp"
#include "rotating_file_sink.hpp"
#include "service_runtime.hpp"
#include "supervisor.hpp"
#include "supervisord_app.hpp"
#include "supervisord_help.hpp"

int runSupervisord(int argc, wchar_t* argv[]) {
    if (argc > 1) {
        std::wstring arg1 = argv[1];
        if (arg1 == L"--help" || arg1 == L"-h" || arg1 == L"help" || arg1 == L"/?" || arg1 == L"-?") {
            PrintHelp();
            return 0;
        } else if (arg1 == L"--version" || arg1 == L"-V" || arg1 == L"-v") {
            PrintVersion();
            return 0;
        } else if (arg1 == L"--check-config") {
            std::wstring configPath = (argc > 2) ? argv[2] : (GetUserHomeDir() + L"\\supervisord.conf");
            auto parsed = ParseConfigDetailed(configPath);
            std::cout << "Config path: " << WideToUtf8(configPath) << "\n";
            for (const auto& w : parsed.warnings) std::cout << "WARN: " << w << "\n";
            for (const auto& e : parsed.errors) std::cout << "ERROR: " << e << "\n";
            std::cout << "Programs loaded: " << parsed.configs.size() << "\n";
            return parsed.errors.empty() ? 0 : 2;
        } else if (arg1 == L"--service") {
            SERVICE_TABLE_ENTRYW ServiceTable[] = {
                { (LPWSTR)L"Supervisord", (LPSERVICE_MAIN_FUNCTIONW)SvcMain },
                { NULL, NULL }
            };
            StartServiceCtrlDispatcherW(ServiceTable);
            return 0;
        } else if (arg1 == L"--install-service") {
            ManageServiceRegistration(true);
            return 0;
        } else if (arg1 == L"--uninstall-service") {
            ManageServiceRegistration(false);
            return 0;
        } else if (arg1 == L"status" || arg1 == L"diag" || arg1 == L"metrics" || arg1 == L"reload" || arg1 == L"start" || arg1 == L"stop" || arg1 == L"restart") {
            std::string cmd = WideToUtf8(arg1);
            for (int i = 2; i < argc; ++i) {
                std::wstring argN = argv[i];
                cmd += " " + QuoteForIpc(WideToUtf8(argN));
            }
            return SendIpcClientCommand(cmd);
        } else {
            std::wcout << L"Unknown command: " << arg1 << L"\n\n";
            PrintHelp();
            return 1;
        }
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    Logger::Log("supervisord", "Starting standalone console daemon...");

    std::wstring configPath = GetUserHomeDir() + L"\\supervisord.conf";
    if (!g_Supervisor.LoadConfiguration(configPath)) {
        std::cout << "Configuration errors prevented startup. Run --check-config for details.\n";
        return 2;
    }
    g_Supervisor.StartAll();
    g_Supervisor.RunIpcServer();

    while (!Supervisord::g_SignalReceived) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    g_Supervisor.Shutdown();
    g_Supervisor.JoinIpcServer();
    g_Supervisor.WriteMetricsFile();
    RotatingFileSink::ReportStats();
    return 0;
}
