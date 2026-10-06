#include "stage_runner.hpp"

bool StageRunner::execute(const std::string& script, const std::string& stageName, const std::string& buildRoot) {
        if (script.empty()) return true;

        std::cout << "Executing (%" << stageName << "): " << "cmd.exe /c ...\n";
        fs::path bat = fs::temp_directory_path() / ("rpmstage_" + stageName + "_" + std::to_string(GetCurrentProcessId()) + ".bat");
        {
            std::ofstream out(bat);
            out << "@echo off\r\n";
            out << "set BUILDROOT=" << buildRoot << "\r\n";
            out << "set RPM_BUILD_ROOT=" << buildRoot << "\r\n";
            out << script << "\r\n";
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi{};
        std::string cmd = "cmd.exe /s /c \"\"" + bat.string() + "\"\"";

        std::vector<char> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        std::error_code ec;
        if (ok) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            fs::remove(bat, ec);
            if (exitCode != 0) {
                std::cerr << "error: Bad exit status from %" << stageName << " (exit code " << exitCode << ")\n";
                return false;
            }
            return true;
        }
        fs::remove(bat, ec);
        return false;
    }
