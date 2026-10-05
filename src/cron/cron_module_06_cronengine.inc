class CronEngine {
private:
    CronOptions options;

    static void executeCommandAsync(const std::string& cmd) {
        std::thread([cmd]() {
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi = { 0 };
            si.cb = sizeof(si);

            std::string fullCmd = "cmd.exe /c " + cmd;

            BOOL success = CreateProcessA(
                NULL, &fullCmd[0], NULL, NULL, FALSE,
                CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
            );

            if (success) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            } else {
                std::cerr << "[-] Execution failed for: " << cmd << std::endl;
            }
        }).detach();
    }

public:
    explicit CronEngine(CronOptions opts) : options(std::move(opts)) {}

    int execute() {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        std::vector<CronJob> jobs = CrontabParser::load(options.crontabPath);

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            CronReporter::dispatchStatus(jobs.size(), options.outputFormat, options.pipeCommand);
        } else {
            std::cout << "\n";
            std::cout << " Cron Task Scheduler\n";
            std::cout << " Target Crontab File: " << options.crontabPath << "\n";
            std::cout << " Press Ctrl+C to stop.\n\n";
            std::cout << "[*] Loaded " << jobs.size() << " active cron job(s).\n";
        }

        FILETIME lastWriteTime = CrontabParser::getFileLastWriteTime(options.crontabPath);
        time_t lastExecutedMinute = 0;

        while (g_cronRunning) {
            FILETIME currentWriteTime = CrontabParser::getFileLastWriteTime(options.crontabPath);
            if (CompareFileTime(&lastWriteTime, &currentWriteTime) != 0) {
                lastWriteTime = currentWriteTime;
                jobs = CrontabParser::load(options.crontabPath);
                std::cout << "[*] Crontab modified. Reloaded " << jobs.size() << " job(s).\n";
            }

            time_t now = time(nullptr);
            time_t currentMinuteTimestamp = (now / 60) * 60;

            if (currentMinuteTimestamp > lastExecutedMinute) {
                lastExecutedMinute = currentMinuteTimestamp;

                struct tm localTime;
                localtime_s(&localTime, &now);

                char timeBuf[64];
                strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTime);

                for (const auto& job : jobs) {
                    if (CronTimeMatcher::isJobDue(job, localTime)) {
                        std::cout << "[" << timeBuf << "] [CRON EXEC] " << job.command << std::endl;
                        executeCommandAsync(job.command);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

