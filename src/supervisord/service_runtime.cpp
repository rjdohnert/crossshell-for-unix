#include "path_encoding.hpp"
#include "rotating_file_sink.hpp"
#include "service_runtime.hpp"
#include "supervisor.hpp"

bool g_IsServiceMode = false;

SERVICE_STATUS        g_SvcStatus = {0};
SERVICE_STATUS_HANDLE g_SvcStatusHandle = NULL;
Supervisord           g_Supervisor;

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl) {
    if (dwCtrl == SERVICE_CONTROL_STOP || dwCtrl == SERVICE_CONTROL_SHUTDOWN) {
        g_SvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
        Supervisord::g_SignalReceived = true;
    }
}

VOID WINAPI SvcMain(DWORD argc, LPTSTR* argv) {
    g_IsServiceMode = true;
    g_SvcStatusHandle = RegisterServiceCtrlHandlerW(L"Supervisord", SvcCtrlHandler);
    if (!g_SvcStatusHandle) return;

    g_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_SvcStatus.dwCurrentState = SERVICE_RUNNING;
    g_SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);

    std::wstring configPath = GetUserHomeDir() + L"\\supervisord.conf";
    if (!g_Supervisor.LoadConfiguration(configPath)) {
        g_SvcStatus.dwCurrentState = SERVICE_STOPPED;
        g_SvcStatus.dwWin32ExitCode = ERROR_INVALID_DATA;
        SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
        return;
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

    g_SvcStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT) {
        Supervisord::g_SignalReceived = true;
        return TRUE;
    }
    // Ignore CTRL_BREAK_EVENT so targeted child process breaks don't kill supervisor
    return TRUE;
}

void ManageServiceRegistration(bool install) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) { std::cout << "Failed to open SCManager.\n"; return; }

    if (install) {
        std::wstring exePath = GetExecutableDir() + L"\\supervisord.exe";
        std::wstring cmd = L"\"" + exePath + L"\" --service";
        SC_HANDLE schService = CreateServiceW(
            schSCManager, L"Supervisord", L"Windows Native Supervisor Service",
            SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL, cmd.c_str(), NULL, NULL, NULL, NULL, NULL
        );
        if (schService) {
            std::cout << "Service installed successfully.\n";
            CloseServiceHandle(schService);
        } else {
            std::cout << "Failed to install service. Error: " << GetLastError() << "\n";
        }
    } else {
        SC_HANDLE schService = OpenServiceW(schSCManager, L"Supervisord", DELETE);
        if (schService) {
            if (DeleteService(schService)) std::cout << "Service uninstalled successfully.\n";
            else std::cout << "Failed to delete service.\n";
            CloseServiceHandle(schService);
        }
    }
    CloseServiceHandle(schSCManager);
}
