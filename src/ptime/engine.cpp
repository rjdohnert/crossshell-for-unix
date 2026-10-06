#include "engine.hpp"

std::wstring ProcessTimerEngine::BuildCommandLine(const std::vector<std::wstring>& args) {
    std::wstring result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) result += L" ";
        std::wstring arg = args[i];
        bool needs_quotes = arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
        if (needs_quotes) {
            result += L"\"";
            for (wchar_t c : arg) {
                if (c == L'"') result += L"\\\"";
                else result += c;
            }
            result += L"\"";
        } else {
            result += arg;
        }
    }
    return result;
}

int ProcessTimerEngine::ExecuteAndMeasure(int argc, wchar_t* argv[], const TimeOptions& options, ProcessTimeMetrics& metrics) {
    if (options.cmdStart >= argc) {
        std::fwprintf(stderr, L"ptime: missing command operand\nTry 'ptime --help' for more information.\n");
        return 1;
    }

    std::vector<std::wstring> child_args;
    for (int i = options.cmdStart; i < argc; ++i) {
        child_args.push_back(argv[i]);
    }

    std::wstring cmd_line = BuildCommandLine(child_args);

    ScopedJobHandle hJob(CreateJobObjectW(NULL, NULL));

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessW(
        NULL,
        &cmd_line[0],
        NULL, NULL, TRUE,
        CREATE_SUSPENDED,
        NULL, NULL, &si, &pi
    );

    if (!success) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            std::vector<std::wstring> cmd_wrapper = { L"cmd.exe", L"/c" };
            cmd_wrapper.insert(cmd_wrapper.end(), child_args.begin(), child_args.end());
            cmd_line = BuildCommandLine(cmd_wrapper);

            success = CreateProcessW(
                NULL,
                &cmd_line[0],
                NULL, NULL, TRUE,
                CREATE_SUSPENDED,
                NULL, NULL, &si, &pi
            );
        }
    }

    if (!success) {
        DWORD err = GetLastError();
        std::fwprintf(stderr, L"time: cannot execute '%s': Win32 error %lu\n", child_args[0].c_str(), err);
        return 127;
    }

    if (hJob.IsValid()) {
        AssignProcessToJobObject(hJob.Get(), pi.hProcess);
    }

    LARGE_INTEGER qpc_freq, qpc_start, qpc_end;
    QueryPerformanceFrequency(&qpc_freq);
    QueryPerformanceCounter(&qpc_start);

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    ScopedProcessHandle hProcess(pi.hProcess);
    WaitForSingleObject(hProcess.Get(), INFINITE);

    QueryPerformanceCounter(&qpc_end);

    GetExitCodeProcess(hProcess.Get(), &metrics.exitCode);

    metrics.realSec = static_cast<double>(qpc_end.QuadPart - qpc_start.QuadPart) / qpc_freq.QuadPart;

    if (hJob.IsValid()) {
        JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION basicIoInfo = {};
        if (QueryInformationJobObject(hJob.Get(), JobObjectBasicAndIoAccountingInformation,
                                      &basicIoInfo, sizeof(basicIoInfo), NULL)) {
            metrics.userSec = static_cast<double>(basicIoInfo.BasicInfo.TotalUserTime.QuadPart) / 10000000.0;
            metrics.sysSec  = static_cast<double>(basicIoInfo.BasicInfo.TotalKernelTime.QuadPart) / 10000000.0;
            metrics.pageFaults = basicIoInfo.BasicInfo.TotalPageFaultCount;
            metrics.totalProcs = basicIoInfo.BasicInfo.TotalProcesses;
            metrics.ioReadOps   = basicIoInfo.IoInfo.ReadOperationCount;
            metrics.ioWriteOps  = basicIoInfo.IoInfo.WriteOperationCount;
            metrics.ioReadBytes  = basicIoInfo.IoInfo.ReadTransferCount;
            metrics.ioWriteBytes = basicIoInfo.IoInfo.WriteTransferCount;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION extLimitInfo = {};
        if (QueryInformationJobObject(hJob.Get(), JobObjectExtendedLimitInformation,
                                      &extLimitInfo, sizeof(extLimitInfo), NULL)) {
            metrics.peakRam = extLimitInfo.PeakJobMemoryUsed;
        }
    }

    return 0;
}
