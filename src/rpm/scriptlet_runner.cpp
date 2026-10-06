#include "scriptlet_runner.hpp"

bool ScriptletRunner::execute(const std::string& script, const std::string& phase) {
        if (script.empty()) return true;

        fs::path tempScript = fs::temp_directory_path() / ("rpm_" + phase + "_" + std::to_string(GetCurrentProcessId()) + ".bat");
        {
            std::ofstream out(tempScript);
            out << "@echo off\r\n" << script << "\r\n";
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi{};
        std::string cmd = "cmd.exe /s /c \"\"" + tempScript.string() + "\"\"";

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
            fs::remove(tempScript, ec);
            return exitCode == 0;
        }
        fs::remove(tempScript, ec);
        return false;
    }
