#include "refresh_options.hpp"
#include "scoped_service_handle.hpp"
#include "service_subsystem_manager.hpp"

bool ServiceSubsystemManager::WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs) {
        DWORD start = GetTickCount();
        SERVICE_STATUS_PROCESS st = {};
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

bool ServiceSubsystemManager::StopThenStart(SC_HANDLE svc, DWORD timeoutMs, const std::wstring& name) {
        SERVICE_STATUS status = {};
        if (!ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
            DWORD err = GetLastError();
            if (err != ERROR_SERVICE_NOT_ACTIVE) {
                std::wcerr << L"refresh: stop failed for '" << name << L"' (error " << err << L")\n";
                return false;
            }
        }

        if (!WaitForState(svc, SERVICE_STOPPED, timeoutMs)) {
            std::wcerr << L"refresh: timed out waiting for stop: " << name << L"\n";
            return false;
        }

        if (!StartServiceW(svc, 0, nullptr)) {
            std::wcerr << L"refresh: start failed for '" << name << L"' (error " << GetLastError() << L")\n";
            return false;
        }

        if (!WaitForState(svc, SERVICE_RUNNING, timeoutMs)) {
            std::wcerr << L"refresh: timed out waiting for running state: " << name << L"\n";
            return false;
        }

        return true;
    }

int ServiceSubsystemManager::RefreshService(const RefreshOptions& opt) {
        if (opt.service.empty()) {
            std::cerr << "refresh: missing required option -s SERVICE\n";
            return 2;
        }

        ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm.IsValid()) {
            std::cerr << "refresh: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScopedServiceHandle svc(OpenServiceW(
            scm.Get(),
            opt.service.c_str(),
            SERVICE_QUERY_STATUS | SERVICE_USER_DEFINED_CONTROL | SERVICE_START | SERVICE_STOP
        ));

        if (!svc.IsValid()) {
            DWORD err = GetLastError();
            std::wcerr << L"refresh: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS status = {};
        if (ControlService(svc.Get(), SERVICE_CONTROL_PARAMCHANGE, &status)) {
            std::wcout << L"0513-095 The subsystem has been refreshed. Subsystem: " << opt.service << L"\n";
            return 0;
        }

        DWORD err = GetLastError();
        if (opt.restartFallback &&
            (err == ERROR_INVALID_SERVICE_CONTROL || err == ERROR_CALL_NOT_IMPLEMENTED || err == ERROR_INVALID_FUNCTION)) {
            if (StopThenStart(svc.Get(), opt.timeoutMs, opt.service)) {
                std::wcout << L"0513-095 The subsystem has been refreshed by restart fallback. Subsystem: " << opt.service << L"\n";
                return 0;
            }
            return 1;
        }

        std::wcerr << L"refresh: service did not accept refresh control for '" << opt.service << L"' (error " << err << L")\n";
        if (!opt.restartFallback) {
            std::wcerr << L"refresh: rerun with --restart to allow restart fallback.\n";
        }

        return 1;
    }
