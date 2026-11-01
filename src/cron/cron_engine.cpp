#include "cron_engine.h"
#include "crontab_parser.h"
#include "cron_time_matcher.h"
#include "cron_reporter.h"

// Static member definitions.
std::set<std::string> CronEngine::m_runningCommands;
std::mutex CronEngine::m_runningMutex;

namespace {

std::atomic<bool> g_cronRunning{true};

BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[*] Shutting down Cron daemon..." << std::endl;
        g_cronRunning = false;
        return TRUE;
    }
    return FALSE;
}

} // namespace

CronEngine::CronEngine(CronOptions opts) : options(std::move(opts)) {}

void CronEngine::executeCommandAsync(const std::string& cmd) {
    // Overlap protection: skip if a previous instance of this exact command is
    // still running. This prevents a fast schedule (e.g. "* * * * *") from
    // stacking up copies of a slow job.
    {
        std::lock_guard<std::mutex> lock(m_runningMutex);
        if (m_runningCommands.count(cmd) != 0) {
            std::cout << "[*] Skipping overlapping instance of: " << cmd << std::endl;
            return;
        }
        m_runningCommands.insert(cmd);
    }

    std::thread([cmd]() {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);

        // Build a writable command-line buffer for CreateProcessA.
        std::string fullCmd = "cmd.exe /c " + cmd;
        std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(
            NULL, cmdBuf.data(), NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
        );

        if (success) {
            // Wait for the job to finish so we can release the overlap guard.
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        } else {
            std::cerr << "[-] Execution failed for: " << cmd << std::endl;
        }

        // Release the overlap guard.
        {
            std::lock_guard<std::mutex> lock(m_runningMutex);
            m_runningCommands.erase(cmd);
        }
    }).detach();
}

int CronEngine::execute() {
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

    // Fire any @reboot jobs once at startup.
    for (const auto& job : jobs) {
        if (job.minutePattern == "@reboot") {
            std::cout << "[*] [@reboot] " << job.command << std::endl;
            executeCommandAsync(job.command);
        }
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
                // @reboot jobs only fire at startup, never on a schedule.
                if (job.minutePattern == "@reboot") continue;

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
