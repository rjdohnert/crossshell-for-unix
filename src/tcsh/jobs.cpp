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
            DWORD code = 0;
            if (GetExitCodeProcess(job.hProcess, &code) && code != STILL_ACTIVE) {
                job.isRunning = false;
                if (notifyJobs) {
                    std::cout << "\n[" << job.id << "] Done\t" << job.command << "\n";
                }
                if (job.hProcess) { CloseHandle(job.hProcess); job.hProcess = NULL; }
                if (job.hJob) { CloseHandle(job.hJob); job.hJob = NULL; }
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

    for (size_t i = 0; i < pipeline.commands.size(); ++i) {
        bool isLast = (i == pipeline.commands.size() - 1);
        ScopedHandle hReadPipe, hWritePipe;

        if (!isLast) {
            HANDLE rPipe = NULL, wPipe = NULL;
            if (CreatePipe(&rPipe, &wPipe, &sa, 0)) {
                hReadPipe.reset(rPipe);
                hWritePipe.reset(wPipe);
                SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0);
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
                }
                continue;
            }

            DWORD desiredAccess = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? FILE_APPEND_DATA : GENERIC_WRITE;
            DWORD creationDisposition = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? OPEN_ALWAYS : CREATE_ALWAYS;

            HANDLE fileHandle = CreateFileW(wTarget.c_str(), desiredAccess, FILE_SHARE_READ, &sa, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle != INVALID_HANDLE_VALUE) {
                redirectedOutput.reset(fileHandle);
                if (isErrRedirect) redirectStderr = true;
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
            SetHandleInformation(hInput.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
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
            finalCmdLine = "\"" + resolvedExe + "\"";
            for (size_t a = 1; a < args.size(); ++a) finalCmdLine += " " + escapeArg(stripOuterQuotes(args[a]));
        } else {
            if (!shouldFallbackToCmd(stripOuterQuotes(args[0]))) {
                print_error_message("tcsh: Command not found: " + args[0] + "\n");
                setVariableList("status", { "1" });
                return;
            }
            finalCmdLine = "cmd.exe /c " + command.sourceText;
        }

        std::wstring wFinalCmdLine = string_to_wstring(finalCmdLine);
        std::vector<wchar_t> cmdBuf(wFinalCmdLine.begin(), wFinalCmdLine.end());
        cmdBuf.push_back(L'\0');

        PROCESS_INFORMATION pi = { 0 };
        DWORD creationFlags = CREATE_NEW_PROCESS_GROUP;
        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, creationFlags, NULL, NULL, &si, &pi)) {
            ScopedHandle hProcess(pi.hProcess);
            ScopedHandle hThread(pi.hThread);
            hInput = std::move(hReadPipe);

            if (pipeline.background && isLast) {
                updateJobs();
                if (jobList.size() >= kMaxJobCount) {
                    print_error_message("tcsh: too many jobs; dropping background job\n");
                    setVariableList("status", { "1" });
                    return;
                }
                HANDLE hJob = CreateJobObjectW(NULL, NULL);
                if (hJob != NULL) {
                    AssignProcessToJobObject(hJob, hProcess.get());
                }
                jobList.push_back({ nextJobId++, hJob, hProcess.detach(), command.sourceText, true });
                setVariableList("status", { "0" });
            } else if (isLast) {
                g_foregroundProcessGroupId.store(pi.dwProcessId, std::memory_order_relaxed);
                WaitForSingleObject(hProcess.get(), INFINITE);
                g_foregroundProcessGroupId.store(0, std::memory_order_relaxed);
                DWORD exitCode = 0;
                if (GetExitCodeProcess(hProcess.get(), &exitCode)) {
                    setVariableList("status", { std::to_string(exitCode) });
                }
            }
        } else {
            print_error_message("tcsh: failed to create process\n");
            setVariableList("status", { "1" });
            return;
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

            WaitForSingleObject(job.hProcess, INFINITE);
            DWORD exitCode = 0;
            if (GetExitCodeProcess(job.hProcess, &exitCode)) {
                setVariableList("status", { std::to_string(exitCode) });
            } else {
                setVariableList("status", { "1" });
            }
            job.isRunning = false;
            if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) { CloseHandle(job.hProcess); job.hProcess = NULL; }
            if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) { CloseHandle(job.hJob); job.hJob = NULL; }
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

