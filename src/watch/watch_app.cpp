#include "argument_formatter.hpp"
#include "command_runner_engine.hpp"
#include "watch_app.hpp"
#include "watch_console_reporter.hpp"
#include "watch_options.hpp"

BOOL WINAPI WatchApplication::CtrlHandler(DWORD type) {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
            ExitProcess(130);
        }
        return FALSE;
    }

int WatchApplication::Run(int argc, wchar_t* argv[]) const {
        SetConsoleCtrlHandler(CtrlHandler, TRUE);

        WatchOptions options;
        int parseStatus = options.Parse(argc, argv);
        if (parseStatus != 0) {
            WatchConsoleReporter::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"watch");
            return parseStatus;
        }

        if (options.showHelp) {
            WatchConsoleReporter::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"watch");
            return 0;
        }
        if (options.showVersion) {
            WatchConsoleReporter::PrintVersion();
            return 0;
        }

        const std::wstring userCommand = ArgumentFormatter::JoinCommandArgs(options.commandArgs);
        const std::wstring wrappedCmd = L"cmd.exe /c " + userCommand;

        while (true) {
            WatchConsoleReporter::ClearScreen();
            WatchConsoleReporter::PrintHeader(options, userCommand);
            CommandRunnerEngine::RunOnce(wrappedCmd);

            const auto sleepStep = std::chrono::milliseconds(100);
            const auto totalSleep = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<double>(options.intervalSeconds)
            );
            auto slept = std::chrono::milliseconds(0);
            while (slept < totalSleep) {
                std::this_thread::sleep_for(sleepStep);
                slept += sleepStep;
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                    return 0;
                }
            }
        }
    }
