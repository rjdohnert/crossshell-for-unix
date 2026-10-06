#include "format_helper.hpp"
#include "job_limit_manager.hpp"
#include "scoped_job_handle.hpp"
#include "scoped_process_handle.hpp"
#include "ulimit_options.hpp"

bool JobLimitManager::SetJobLimits(HANDLE job_handle, const UlimitOptions& options) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = 0;

        if (options.kill_on_close) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        }

        if (options.priority_class != 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PRIORITY_CLASS;
            info.BasicLimitInformation.PriorityClass = options.priority_class;
        }

        if (options.memory_megabytes.has_value() && options.memory_megabytes.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            info.ProcessMemoryLimit = static_cast<SIZE_T>(options.memory_megabytes.value() * 1024ULL * 1024ULL);
        }

        if (options.virtual_mem_mb.has_value() && options.virtual_mem_mb.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
            info.JobMemoryLimit = static_cast<SIZE_T>(options.virtual_mem_mb.value() * 1024ULL * 1024ULL);
        }

        if (options.working_set_mb.has_value() && options.working_set_mb.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_WORKINGSET;
            info.BasicLimitInformation.MinimumWorkingSetSize = static_cast<SIZE_T>(1024 * 1024);
            info.BasicLimitInformation.MaximumWorkingSetSize = static_cast<SIZE_T>(options.working_set_mb.value() * 1024ULL * 1024ULL);
        }

        if (options.cpu_seconds.has_value() && options.cpu_seconds.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
            info.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = static_cast<LONGLONG>(options.cpu_seconds.value() * 10000000ULL);
        }

        if (options.process_count.has_value() && options.process_count.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            info.BasicLimitInformation.ActiveProcessLimit = options.process_count.value();
        }

        if (info.BasicLimitInformation.LimitFlags == 0) {
            return true;
        }

        return SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &info, sizeof(info)) != FALSE;
    }

bool JobLimitManager::RunChildUnderLimits(const UlimitOptions& options) {
        if (options.command.empty()) return true;

        std::wstring command_line = FormatHelper::BuildCommandLine(options.command);
        std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
        mutable_command.push_back(L'\0');

        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process_info = {};

        ScopedJobHandle job_handle(CreateJobObjectW(nullptr, nullptr));
        if (!job_handle.IsValid()) {
            std::wcerr << L"ulimit: unable to create job object\n";
            return false;
        }

        if (!SetJobLimits(job_handle.Get(), options)) {
            std::wcerr << L"ulimit: unable to apply job limits\n";
            return false;
        }

        DWORD creation_flags = CREATE_SUSPENDED;
        if (options.priority_class != 0) {
            creation_flags |= options.priority_class;
        }

        if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
                            creation_flags, nullptr, nullptr, &startup, &process_info)) {
            std::wcerr << L"ulimit: unable to start command: " << command_line << L"\n";
            return false;
        }

        ScopedProcessHandle hProcess(process_info.hProcess);
        ScopedProcessHandle hThread(process_info.hThread);

        if (!AssignProcessToJobObject(job_handle.Get(), hProcess.Get())) {
            std::wcerr << L"ulimit: unable to assign command to job object\n";
            TerminateProcess(hProcess.Get(), 1);
            return false;
        }

        ResumeThread(hThread.Get());
        WaitForSingleObject(hProcess.Get(), INFINITE);

        DWORD exit_code = 1;
        GetExitCodeProcess(hProcess.Get(), &exit_code);

        return exit_code == 0;
    }
