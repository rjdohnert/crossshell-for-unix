#include "command_line_escaper.hpp"
#include "process_batch_runner.hpp"

int ProcessBatchRunner::runCommand(const std::vector<std::string>& cmdArgs, bool verbose) {
        std::string cmdLine = CommandLineEscaper::build(cmdArgs);

        if (verbose) {
            std::cerr << cmdLine << "\n";
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        if (!ok) {
            std::cerr << "xargs: " << cmdArgs[0] << ": cannot execute\n";
            return 127;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
