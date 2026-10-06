#include "toolchain_locator.hpp"

bool ToolchainLocator::IsClInPath() const {
        return SearchPathA(NULL, "cl.exe", NULL, 0, NULL, NULL) > 0;
    }

std::string ToolchainLocator::FindVsInstallation() const {
        std::string vswherePath = "vswhere.exe";
        bool hasVswhereInPath = (SearchPathA(NULL, "vswhere.exe", NULL, 0, NULL, NULL) > 0);

        if (!hasVswhereInPath) {
            char pf[MAX_PATH] = {};
            if (GetEnvironmentVariableA("ProgramFiles(x86)", pf, MAX_PATH) > 0) {
                std::string p = std::string(pf) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
                if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    vswherePath = p;
                }
            }
            if (vswherePath == "vswhere.exe" && GetEnvironmentVariableA("ProgramFiles", pf, MAX_PATH) > 0) {
                std::string p = std::string(pf) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
                if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    vswherePath = p;
                }
            }
        }

        std::string command = "\"" + vswherePath + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";

        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe) return "";

        char buffer[512];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        _pclose(pipe);

        size_t end = result.find_last_not_of(" \n\r\t");
        return (end == std::string::npos) ? "" : result.substr(0, end + 1);
    }

std::string ToolchainLocator::GetVcvarsBatchPath(const std::string& vsPath, TargetArch arch) const {
        std::string scriptName = "vcvars64.bat";
        if (arch == TargetArch::X86) {
            scriptName = "vcvars32.bat";
        } else if (arch == TargetArch::ARM64) {
            scriptName = "vcvarsarm64.bat";
        }
        return vsPath + "\\VC\\Auxiliary\\Build\\" + scriptName;
    }
