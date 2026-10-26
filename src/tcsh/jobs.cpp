#include "engine.hpp"
#include "jobs.hpp"

std::atomic<bool> g_interrupted{false};
std::atomic<DWORD> g_foregroundProcessGroupId{0};
std::atomic<bool> g_ctrlForwarding{false};

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        DWORD processGroupId = g_foregroundProcessGroupId.load(std::memory_order_relaxed);
        if (processGroupId != 0 && !g_ctrlForwarding.exchange(true, std::memory_order_relaxed)) {
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, processGroupId);
            g_ctrlForwarding.store(false, std::memory_order_relaxed);
        }
        g_interrupted.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}


void TcshEngine::updateJobs() {
    for (auto& job : jobList) {
        if (job.isRunning && job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
            bool all_processes_finished = true;
            std::vector<HANDLE> process_handles = job.stageProcesses;
            process_handles.push_back(job.hProcess);
            for (HANDLE process_handle : process_handles) {
                DWORD code = 0;
                if (!GetExitCodeProcess(process_handle, &code) || code == STILL_ACTIVE) {
                    all_processes_finished = false;
                    break;
                }
            }
            if (all_processes_finished) {
                job.isRunning = false;
                if (notifyJobs) {
                    std::cout << "\n[" << job.id << "] Done\t" << job.command << "\n";
                }
                close_job_handles(job);
            }
        }
    }

    // Reclaim finished slots to avoid exhausting the fixed job table with stale entries.
    jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [](const Job& job) {
        return !job.isRunning;
    }), jobList.end());
}


void TcshEngine::executePipeline(const ParsedPipeline& pipeline) {
    ScopedHandle hInput;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    std::vector<ScopedHandle> pipelineProcesses;
    ScopedHandle pipelineJob;

    if (pipeline.background) {
        updateJobs();
        if (jobList.size() >= kMaxJobCount) {
            print_error_message("tcsh: too many jobs; cannot start background pipeline\n");
            setVariableList("status", { "1" });
            return;
        }
        pipelineJob.reset(CreateJobObjectW(NULL, NULL));
        if (!pipelineJob.isValid()) {
            print_error_message("tcsh: failed to create background job\n");
            setVariableList("status", { "1" });
            return;
        }
    }

    auto abort_pipeline = [&](const std::string& message) {
        if (!message.empty()) print_error_message(message);
        if (pipelineJob.isValid()) TerminateJobObject(pipelineJob.get(), 1);
        for (const ScopedHandle& process : pipelineProcesses) {
            if (process.isValid()) TerminateProcess(process.get(), 1);
        }
        for (const ScopedHandle& process : pipelineProcesses) {
            if (process.isValid()) WaitForSingleObject(process.get(), INFINITE);
        }
        setVariableList("status", { "1" });
    };

    for (size_t i = 0; i < pipeline.commands.size(); ++i) {
        bool isLast = (i == pipeline.commands.size() - 1);
        ScopedHandle hReadPipe, hWritePipe;

        if (!isLast) {
            HANDLE rPipe = NULL, wPipe = NULL;
            if (CreatePipe(&rPipe, &wPipe, &sa, 0)) {
                hReadPipe.reset(rPipe);
                hWritePipe.reset(wPipe);
                if (!SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0)) {
                    abort_pipeline("tcsh: failed to configure pipeline handles\n");
                    return;
                }
            } else {
                abort_pipeline("tcsh: failed to create pipeline\n");
                return;
            }
        }

        const ParsedCommand& command = pipeline.commands[i];
        std::vector<std::string> args = expandGlobs(command.args);
        if (args.empty()) continue;

        ScopedHandle redirectedInput, redirectedOutput;
        bool redirectStderr = pipeline.pipeStderr;

        for (const auto& redirection : command.redirections) {
            std::wstring wTarget = string_to_wstring(stripOuterQuotes(redirection.target));
            bool isErrRedirect = (redirection.kind == ParsedTokenKind::RedirectOutputStderr || redirection.kind == ParsedTokenKind::RedirectAppendStderr);

            if (redirection.kind == ParsedTokenKind::RedirectInput) {
                HANDLE fileHandle = CreateFileW(wTarget.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (fileHandle != INVALID_HANDLE_VALUE) {
                    redirectedInput.reset(fileHandle);
                } else {
                    abort_pipeline("tcsh: cannot open input redirection: " + redirection.target + "\n");
                    return;
                }
                continue;
            }

            DWORD desiredAccess = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? FILE_APPEND_DATA : GENERIC_WRITE;
            DWORD creationDisposition = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? OPEN_ALWAYS : CREATE_ALWAYS;

            HANDLE fileHandle = CreateFileW(wTarget.c_str(), desiredAccess, FILE_SHARE_READ, &sa, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle != INVALID_HANDLE_VALUE) {
                redirectedOutput.reset(fileHandle);
                if (isErrRedirect) redirectStderr = true;
            } else {
                abort_pipeline("tcsh: cannot open output redirection: " + redirection.target + "\n");
                return;
            }
        }

        // Support Built-in Commands in Pipelines
        if (isBuiltinCommand(args[0])) {
            std::stringstream ss;
            std::streambuf* oldCout = nullptr;
            if (!isLast || redirectedOutput.isValid()) {
                oldCout = std::cout.rdbuf(ss.rdbuf());
            }

            bool handledBuiltin = executeBuiltin(args);

            if (oldCout != nullptr) {
                std::cout.rdbuf(oldCout);
            }

            if (handledBuiltin) {
                if (!isLast || redirectedOutput.isValid()) {
                    std::string builtinOutput = ss.str();
                    HANDLE hOutHandle = redirectedOutput.isValid() ? redirectedOutput.get() : hWritePipe.get();
                    DWORD dwWritten = 0;
                    WriteFile(hOutHandle, builtinOutput.c_str(), static_cast<DWORD>(builtinOutput.length()), &dwWritten, NULL);
                }
                hInput = std::move(hReadPipe);
                continue;
            }
        }

        // Enable inheritance on hInput so CreateProcessW can pass standard input to Child process
        if (hInput.isValid()) {
            if (!SetHandleInformation(hInput.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                abort_pipeline("tcsh: failed to configure pipeline input\n");
                return;
            }
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = redirectedInput.isValid() ? redirectedInput.get() : (hInput.isValid() ? hInput.get() : GetStdHandle(STD_INPUT_HANDLE));
        si.hStdOutput = redirectedOutput.isValid() ? redirectedOutput.get() : ((!isLast) ? hWritePipe.get() : GetStdHandle(STD_OUTPUT_HANDLE));
        si.hStdError = redirectStderr ? si.hStdOutput : GetStdHandle(STD_ERROR_HANDLE);

        bool foundOnDisk = false;
        std::string resolvedExe = resolveExecutable(stripOuterQuotes(args[0]), foundOnDisk);
        std::string finalCmdLine = "";

        if (foundOnDisk) {
            resolvedExe = stripOuterQuotes(resolvedExe);
            finalCmdLine = "\"" + resolvedExe + "\"";
            for (size_t a = 1; a < args.size(); ++a) finalCmdLine += " " + escapeArg(stripOuterQuotes(args[a]));
        } else {
            if (!shouldFallbackToCmd(stripOuterQuotes(args[0]))) {
                abort_pipeline("tcsh: Command not found: " + args[0] + "\n");
                return;
            }
            wchar_t systemDirectory[MAX_PATH] = {};
            UINT systemLength = GetSystemDirectoryW(systemDirectory, MAX_PATH);
            if (systemLength == 0 || systemLength >= MAX_PATH) {
                abort_pipeline("tcsh: unable to locate system cmd.exe\n");
                return;
            }
            std::wstring systemCmdPath(systemDirectory, systemLength);
            if (!systemCmdPath.empty() && systemCmdPath.back() != L'\\') systemCmdPath += L'\\';
            systemCmdPath += L"cmd.exe";
            resolvedExe = wstring_to_string(systemCmdPath);
            finalCmdLine = "\"" + resolvedExe + "\" /d /c " + command.sourceText;
        }

        std::wstring wFinalCmdLine = string_to_wstring(finalCmdLine);
        std::vector<wchar_t> cmdBuf(wFinalCmdLine.begin(), wFinalCmdLine.end());
        cmdBuf.push_back(L'\0');

        PROCESS_INFORMATION pi = { 0 };
        DWORD creationFlags = CREATE_NEW_PROCESS_GROUP;
        std::wstring applicationName = string_to_wstring(resolvedExe);
        if (!CreateProcessW(applicationName.empty() ? NULL : applicationName.c_str(), cmdBuf.data(), NULL, NULL,
                            TRUE, creationFlags, NULL, NULL, &si, &pi)) {
            abort_pipeline("tcsh: failed to create process\n");
            return;
        }

        pipelineProcesses.emplace_back(pi.hProcess);
        ScopedHandle hThread(pi.hThread);
        if (pipeline.background && !AssignProcessToJobObject(pipelineJob.get(), pi.hProcess)) {
            abort_pipeline("tcsh: failed to add process to background job\n");
            return;
        }
        hInput = std::move(hReadPipe);
    }

    if (pipeline.background) {
        if (!pipelineProcesses.empty()) {
            Job job{};
            job.id = nextJobId++;
            job.hJob = pipelineJob.detach();
            job.hProcess = pipelineProcesses.back().detach();
            for (size_t process_index = 0; process_index + 1 < pipelineProcesses.size(); ++process_index) {
                job.stageProcesses.push_back(pipelineProcesses[process_index].detach());
            }
            job.command = pipeline.commands.back().sourceText;
            job.isRunning = true;
            jobList.push_back(std::move(job));
            setVariableList("status", { "0" });
        }
        return;
    }

    if (!pipelineProcesses.empty()) {
        HANDLE lastProcess = pipelineProcesses.back().get();
        g_foregroundProcessGroupId.store(GetProcessId(lastProcess), std::memory_order_relaxed);
        for (const ScopedHandle& process : pipelineProcesses) {
            WaitForSingleObject(process.get(), INFINITE);
        }
        g_foregroundProcessGroupId.store(0, std::memory_order_relaxed);
        DWORD exitCode = 0;
        const bool last_stage_is_builtin = !pipeline.commands.empty() &&
            !pipeline.commands.back().args.empty() && isBuiltinCommand(pipeline.commands.back().args[0]);
        if (!last_stage_is_builtin && GetExitCodeProcess(lastProcess, &exitCode)) {
            setVariableList("status", { std::to_string(exitCode) });
        }
    }
}


bool TcshEngine::listJobs(const std::vector<std::string>& args) {
    updateJobs();

    bool longFormat = false;
    bool pidOnly = false;
    bool endOfOptions = false;
    std::vector<DWORD> requestedJobIds;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string token = stripOuterQuotes(args[i]);
        if (!endOfOptions && token == "--") {
            endOfOptions = true;
            continue;
        }
        if (!endOfOptions && token.size() > 1 && token[0] == '-') {
            for (size_t flagIndex = 1; flagIndex < token.size(); ++flagIndex) {
                if (token[flagIndex] == 'l') longFormat = true;
                else if (token[flagIndex] == 'p') pidOnly = true;
                else {
                    print_error_message("jobs: invalid option: " + token + "\n");
                    return false;
                }
            }
            continue;
        }

        std::string jobSpec = token;
        if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
        int parsed = 0;
        if (!tryParseNonNegativeInt(jobSpec, parsed, "jobs", "job id") || parsed <= 0) return false;
        requestedJobIds.push_back(static_cast<DWORD>(parsed));
    }

    for (const auto& job : jobList) {
        if (!requestedJobIds.empty() &&
            std::find(requestedJobIds.begin(), requestedJobIds.end(), job.id) == requestedJobIds.end()) {
            continue;
        }

        DWORD pid = (job.hProcess != NULL) ? GetProcessId(job.hProcess) : 0;
        if (pidOnly) {
            std::cout << pid << "\n";
        } else if (longFormat) {
            std::cout << "[" << job.id << "] " << pid << " "
                      << (job.isRunning ? "Running" : "Done") << "\t" << job.command << "\n";
        } else {
            std::cout << "[" << job.id << "] " << (job.isRunning ? "Running" : "Done") << "\t" << job.command << "\n";
        }
    }
    return true;
}

void TcshEngine::bringJobToForeground(DWORD jobId) {
    for (auto& job : jobList) {
        if (job.id == jobId) {
            if (!job.isRunning || !job.hProcess) {
                print_error_message("fg: job not running\n");
                setVariableList("status", { "1" });
                return;
            }

            wait_job_processes(job);
            DWORD exitCode = 0;
            if (GetExitCodeProcess(job.hProcess, &exitCode)) {
                setVariableList("status", { std::to_string(exitCode) });
            } else {
                setVariableList("status", { "1" });
            }
            job.isRunning = false;
            close_job_handles(job);
            return;
        }
    }

    print_error_message("fg: No such job\n");
    setVariableList("status", { "1" });
}

void TcshEngine::sendJobToBackground(DWORD jobId) {
    for (auto& job : jobList) {
        if (job.id == jobId) {
            if (!job.isRunning) {
                print_error_message("bg: job already finished\n");
                setVariableList("status", { "1" });
                return;
            }

            std::cout << "[" << job.id << "] " << job.command << " &\n";
            setVariableList("status", { "0" });
            return;
        }
    }

    print_error_message("bg: No such job\n");
    setVariableList("status", { "1" });
}

