#include "service_controller.hpp"

bool ServiceController::WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs) {
        DWORD start = GetTickCount();
        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;

        while (true) {
            if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
                return false;
            }
            if (st.dwCurrentState == wantedState) {
                return true;
            }
            if (GetTickCount() - start > timeoutMs) {
                return false;
            }
            Sleep(250);
        }
    }
