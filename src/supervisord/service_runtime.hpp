#pragma once

#include "supervisor.hpp"
#include "supervisord.hpp"

extern SERVICE_STATUS g_SvcStatus;
extern SERVICE_STATUS_HANDLE g_SvcStatusHandle;
extern Supervisord g_Supervisor;

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl);

VOID WINAPI SvcMain(DWORD argc, LPTSTR* argv);

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

void ManageServiceRegistration(bool install);
