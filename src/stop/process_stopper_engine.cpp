#include "privilege_escalator.hpp"
#include "process_stopper_engine.hpp"
#include "scoped_process_handle.hpp"
#include "stop_options.hpp"
#include "stop_reporter.hpp"

BOOL CALLBACK ProcessStopperEngine::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        EnumData* data = reinterpret_cast<EnumData*>(lParam);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        
        if (windowPid == data->pid && IsWindowVisible(hwnd)) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            data->windowFound = true;
        }
        return TRUE;
    }

bool ProcessStopperEngine::ForceStopProcess(DWORD pid) {
        ScopedProcessHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
        if (!hProcess.IsValid()) return false;

        return TerminateProcess(hProcess.Get(), 1) != FALSE;
    }

bool ProcessStopperEngine::GracefulStopProcess(DWORD pid) {
        EnumData data = { pid, false };
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

        if (!data.windowFound) {
            return ForceStopProcess(pid);
        }
        return true;
    }

bool ProcessStopperEngine::SendConsoleEvent(DWORD pid, DWORD ctrlEvent) {
        if (AttachConsole(pid)) {
            SetConsoleCtrlHandler(NULL, TRUE);
            BOOL res = GenerateConsoleCtrlEvent(ctrlEvent, 0);
            FreeConsole();
            SetConsoleCtrlHandler(NULL, FALSE);
            return res != FALSE;
        }
        return false;
    }

int ProcessStopperEngine::Execute(const StopOptions& options) {
        PrivilegeEscalator::EnableDebugPrivilege();

        bool hasErrors = false;

        for (DWORD pid : options.pids) {
            bool success = false;

            switch (options.signal) {
                case SignalType::SIGHUP:
                case SignalType::SIGTERM:
                    success = GracefulStopProcess(pid);
                    break;
                case SignalType::SIGINT:
                    success = SendConsoleEvent(pid, CTRL_C_EVENT);
                    break;
                case SignalType::SIGQUIT:
                    success = SendConsoleEvent(pid, CTRL_BREAK_EVENT);
                    break;
                case SignalType::SIGstop:
                    success = ForceStopProcess(pid);
                    break;
            }

            if (!success) {
                DWORD err = GetLastError();
                std::cerr << "stop: (" << pid << ") - ";
                if (err == ERROR_INVALID_PARAMETER || err == ERROR_PROC_NOT_FOUND || err == ERROR_INVALID_HANDLE) {
                    std::cerr << "No such process\n";
                } else if (err == ERROR_ACCESS_DENIED) {
                    std::cerr << "Operation not permitted\n";
                } else {
                    std::cerr << "Error code " << err << "\n";
                }
                hasErrors = true;
            }
        }

        StopReporter::Report(options.outputFormat, options.pipeCommand, options.pids.size(), hasErrors);
        return hasErrors ? 1 : 0;
    }
