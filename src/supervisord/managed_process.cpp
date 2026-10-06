#include "async_pipe_pump.hpp"
#include "environment_block.hpp"
#include "health_check_scheduler.hpp"
#include "logger.hpp"
#include "managed_process.hpp"
#include "path_encoding.hpp"
#include "process_state.hpp"
#include "program_config.hpp"
#include "scoped_handle.hpp"

ManagedProcess::ManagedProcess(const ProgramConfig& cfg) : config(cfg), state_(ProcessState::STOPPED) {}

ManagedProcess::~ManagedProcess() { Stop(); }

void ManagedProcess::Start() {
        std::lock_guard<std::mutex> lifecycleLock(lifecycle_mtx_);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (state_ == ProcessState::RUNNING || state_ == ProcessState::STARTING) return;
            should_run_ = true;
            retry_count_ = 0;
        }
        JoinWorkerThread();
        worker_thread_ = std::thread(&ManagedProcess::RunLoop, this);
    }

void ManagedProcess::Stop() {
        std::lock_guard<std::mutex> lifecycleLock(lifecycle_mtx_);
        should_run_ = false;
        StopHealthMonitor();
        bool shouldKill = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (state_ == ProcessState::RUNNING || state_ == ProcessState::STARTING) {
                state_ = ProcessState::STOPPING;
                shouldKill = true;
            }
        }
        if (shouldKill) KillProcessTree();
        JoinWorkerThread();
        StopPipeLogging();
        hStdOutRead_.Close();
        hStdErrRead_.Close();
        state_ = ProcessState::STOPPED;
    }

ProcessState ManagedProcess::GetState() const { return state_; }

std::wstring ManagedProcess::GetName() const { return config.name; }

void ManagedProcess::JoinWorkerThread() {
        if (worker_thread_.joinable() && worker_thread_.get_id() != std::this_thread::get_id()) {
            worker_thread_.join();
        }
    }

void ManagedProcess::RequestHealthStop() {
        stop_health_ = true;
    }

void ManagedProcess::StartPipeLogging(HANDLE hReadPipe, const std::wstring& path, size_t maxBytes, int backups) {
        if (hReadPipe == NULL || hReadPipe == INVALID_HANDLE_VALUE) return;
        if (!AsyncPipePump::Instance().RegisterPipe(hReadPipe, path, maxBytes, backups)) {
            Log("Failed to register async log pipe.");
        }
    }

void ManagedProcess::StopPipeLogging() {
        if (hStdOutRead_.isValid()) {
            AsyncPipePump::Instance().UnregisterPipe(hStdOutRead_.get());
        }
        if (hStdErrRead_.isValid()) {
            AsyncPipePump::Instance().UnregisterPipe(hStdErrRead_.get());
        }
    }

void ManagedProcess::StopHealthMonitor() {
        RequestHealthStop();
        if (health_registered_.exchange(false)) {
            HealthCheckScheduler::Instance().UnregisterProcess(this);
        }
    }

void ManagedProcess::Log(const std::string& msg) {
        std::string nameStr = WideToUtf8(config.name);
        Logger::Log(nameStr, msg);
    }

void ManagedProcess::CreateJob() {
        hJob_ = CreateJobObjectW(NULL, NULL);
        if (hJob_.isValid()) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob_.get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }

bool ManagedProcess::RunBestEffortStopCommand() {
        if (config.stop_command.empty()) return false;

        std::vector<wchar_t> cmdBuffer(config.stop_command.begin(), config.stop_command.end());
        cmdBuffer.push_back(L'\0');

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {0};
        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();

        BOOL ok = CreateProcessW(
            NULL,
            cmdBuffer.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            pDir,
            &si,
            &pi
        );
        if (!ok) {
            Log("stop_command failed to launch. Win32 Error: " + std::to_string(GetLastError()));
            return false;
        }

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThread(pi.hThread);
        WaitForSingleObject(hProc.get(), 2000);
        Log("stop_command executed before shutdown signal.");
        return true;
    }

int ManagedProcess::ComputeBackoffMs(int attempt) {
        int exp = std::min(attempt, 6);
        int baseMs = 500 * (1 << exp);
        baseMs = std::min(baseMs, 30000);
        static thread_local std::mt19937 rng(static_cast<unsigned int>(GetTickCount64()));
        std::uniform_int_distribution<int> jitter(0, 500);
        return baseMs + jitter(rng);
    }

bool ManagedProcess::RunHealthCheckOnce() {
        if (config.healthcheck_command.empty()) return true;

        std::vector<wchar_t> cmdBuffer(config.healthcheck_command.begin(), config.healthcheck_command.end());
        cmdBuffer.push_back(L'\0');
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {0};
        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();

        BOOL ok = CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, pDir, &si, &pi);
        if (!ok) return false;

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThread(pi.hThread);
        WaitForSingleObject(hProc.get(), 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(hProc.get(), &exitCode);
        return exitCode == 0;
    }

void ManagedProcess::StartHealthMonitor() {
        stop_health_ = false;
        if (config.healthcheck_command.empty() || config.healthcheck_interval <= 0) return;
        health_failures_ = 0;
        if (!health_registered_.exchange(true)) {
            HealthCheckScheduler::Instance().RegisterProcess(this, config.healthcheck_interval);
        }
    }

void ManagedProcess::OnSharedHealthCheckTick() {
        if (stop_health_.load() || !should_run_.load()) return;

        if (RunHealthCheckOnce()) {
            health_failures_ = 0;
            return;
        }

        int consecutiveFailures = ++health_failures_;
        g_totalHealthCheckFailures++;
        Log("Health check failed (" + std::to_string(consecutiveFailures) + ")");

        if (consecutiveFailures >= std::max(1, config.healthcheck_failures)) {
            Log("Health check threshold reached. Terminating process for restart policy.");
            if (hProcess_.isValid()) {
                TerminateProcess(hProcess_.get(), 87);
            }
        }
    }

bool ManagedProcess::Launch() {
        CreateJob();

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
        HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;

        if (!config.stdout_logfile.empty()) {
            CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            hStdOutRead_ = hStdOutRead;
            StartPipeLogging(hStdOutRead_.get(), config.stdout_logfile, config.stdout_logfile_maxbytes, config.stdout_logfile_backups);
        }

        if (!config.stderr_logfile.empty()) {
            CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
            SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
            hStdErrRead_ = hStdErrRead;
            StartPipeLogging(hStdErrRead_.get(), config.stderr_logfile, config.stderr_logfile_maxbytes, config.stderr_logfile_backups);
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hStdOutWrite ? hStdOutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = hStdErrWrite ? hStdErrWrite : GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuffer(config.command.begin(), config.command.end());
        cmdBuffer.push_back(L'\0');

        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();
        auto envBlock = CreateEnvironmentBlock(config.environment);

        DWORD creationFlags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;

        BOOL success = CreateProcessW(
            NULL, cmdBuffer.data(), NULL, NULL, TRUE,
            creationFlags, envBlock.data(), pDir, &si, &pi
        );

        if (hStdOutWrite) CloseHandle(hStdOutWrite);
        if (hStdErrWrite) CloseHandle(hStdErrWrite);

        if (!success) {
            g_totalLaunchFailures++;
            Log("Failed to launch process. Win32 Error: " + std::to_string(GetLastError()));
            StopPipeLogging();
            hStdOutRead_.Close();
            hStdErrRead_.Close();
            return false;
        }

        g_totalStarts++;

        hProcess_ = pi.hProcess;
        ScopedHandle hThread(pi.hThread);

        if (hJob_.isValid()) AssignProcessToJobObject(hJob_.get(), hProcess_.get());
        ResumeThread(hThread.get());
        return true;
    }

void ManagedProcess::KillProcessTree() {
        if (!hProcess_.isValid()) return;
        DWORD pid = GetProcessId(hProcess_.get());

        RunBestEffortStopCommand();

        if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
            // Only attempt to attach when we are not already bound to a console.
            if (GetConsoleWindow() == NULL) {
                if (AttachConsole(pid)) {
                    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
                    FreeConsole();
                }
            }
        }

        if (WaitForSingleObject(hProcess_.get(), config.stoptimeout * 1000) == WAIT_TIMEOUT) {
            Log("Timeout reached. Force terminating process tree...");
            TerminateProcess(hProcess_.get(), 1);
        }

        if (hJob_.isValid()) hJob_.Close();
        hProcess_.Close();

        StopHealthMonitor();
        StopPipeLogging();

        // Close read pipes to unblock ReadFile if grandchild process inherited write pipe
        hStdOutRead_.Close();
        hStdErrRead_.Close();
    }

void ManagedProcess::RunLoop() {
        while (should_run_) {
            state_ = (retry_count_ == 0) ? ProcessState::STARTING : ProcessState::BACKOFF;
            Log(state_ == ProcessState::STARTING ? "Starting process..." : "Restarting (Attempt " + std::to_string(retry_count_) + ")...");

            if (!Launch()) {
                retry_count_++;
                if (retry_count_ > config.startretries) {
                    state_ = ProcessState::FATAL;
                    Log("Process entered FATAL state.");
                    should_run_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ComputeBackoffMs(retry_count_)));
                continue;
            }

            state_ = ProcessState::RUNNING;
            Log("Process running cleanly.");
            StartHealthMonitor();

            WaitForSingleObject(hProcess_.get(), INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(hProcess_.get(), &exitCode);
            hProcess_.Close();

            // Explicitly close read pipes so logging threads exit cleanly
            StopPipeLogging();
            hStdOutRead_.Close();
            hStdErrRead_.Close();
            StopHealthMonitor();

            if (!should_run_) {
                state_ = ProcessState::STOPPED;
                Log("Process stopped by user request.");
                break;
            }

            Log("Process terminated with exit code " + std::to_string(exitCode));
            g_totalUnexpectedExits++;

            bool isExpected = std::find(config.exitcodes.begin(), config.exitcodes.end(), exitCode) != config.exitcodes.end();
            bool shouldRestart = (config.autorestart == L"true" || config.autorestart == L"1") ||
                                 (config.autorestart == L"unexpected" && !isExpected);

            if (shouldRestart) {
                retry_count_++;
                g_totalRestarts++;
                if (retry_count_ > config.startretries) {
                    state_ = ProcessState::FATAL;
                    Log("Exceeded max startretries. Entering FATAL state.");
                    should_run_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ComputeBackoffMs(retry_count_)));
            } else {
                state_ = ProcessState::STOPPED;
                should_run_ = false;
                break;
            }
        }
    }
