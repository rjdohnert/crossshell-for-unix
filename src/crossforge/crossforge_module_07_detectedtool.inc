    struct DetectedTool {
        std::string Name;
        std::string Path;
        bool Found;
    };

    static std::vector<DetectedTool> DetectAll() {
        std::vector<DetectedTool> results;

        // Check Visual Studio Build Tools / MSVC in Program Files
        std::string vsPath = SearchVisualStudio();

        std::vector<std::pair<std::string, std::string>> executables = {
            {"Microsoft Visual C++ (cl.exe)", "cl.exe"},
            {"GCC Compiler (gcc.exe)", "gcc.exe"},
            {"G++ Compiler (g++.exe)", "g++.exe"},
            {"Rust Compiler (rustc.exe)", "rustc.exe"},
            {"Go Toolchain (go.exe)", "go.exe"},
            {"Java Compiler (javac.exe)", "javac.exe"},
            {"C# Compiler (csc.exe)", "csc.exe"},
            {"Free Pascal Compiler (fpc.exe)", "fpc.exe"},
            {"GNU Fortran (gfortran.exe)", "gfortran.exe"},
            {"GnuCOBOL (cobc.exe)", "cobc.exe"},
            {"Node.js Runtime (node.exe)", "node.exe"},
            {"TypeScript Compiler (tsc.cmd)", "tsc.cmd"},
            {"Swift Compiler (swiftc.exe)", "swiftc.exe"}
        };

        for (const auto& tool : executables) {
            std::string foundPath = FindInPath(tool.second);
            if (foundPath.empty() && tool.second == "cl.exe" && !vsPath.empty()) {
                foundPath = vsPath;
            }
            results.push_back({tool.first, foundPath, !foundPath.empty()});
        }

        return results;
    }

    static std::vector<std::string> GetVisualStudioBuildToolDirs() {
        std::vector<std::string> result;
        const char* programFilesEnv[] = {"ProgramFiles", "ProgramFiles(x86)"};
        const std::vector<std::string> years = {"2022", "2019", "2017"};
        const std::vector<std::string> editions = {"Enterprise", "Professional", "Community", "BuildTools"};

        for (const char* envVar : programFilesEnv) {
            char pf[MAX_PATH];
            if (GetEnvironmentVariableA(envVar, pf, MAX_PATH) == 0) {
                continue;
            }

            fs::path base(pf);
            base /= "Microsoft Visual Studio";
            for (const auto& year : years) {
                for (const auto& ed : editions) {
                    fs::path msvcRoot = base / year / ed / "VC/Tools/MSVC";
                    if (!fs::exists(msvcRoot) || !fs::is_directory(msvcRoot)) {
                        continue;
                    }

                    for (const auto& entry : fs::directory_iterator(msvcRoot)) {
                        if (!entry.is_directory()) {
                            continue;
                        }

                        fs::path hostX64x64 = entry.path() / "bin/Hostx64/x64";
                        fs::path hostX64x86 = entry.path() / "bin/Hostx64/x86";
                        fs::path hostX86x86 = entry.path() / "bin/Hostx86/x86";

                        if (fs::exists(hostX64x64 / "cl.exe")) result.push_back(hostX64x64.string());
                        if (fs::exists(hostX64x86 / "cl.exe")) result.push_back(hostX64x86.string());
                        if (fs::exists(hostX86x86 / "cl.exe")) result.push_back(hostX86x86.string());
                    }
                }
            }
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    // Locate a Visual Studio developer-environment activation script so builds can
    // set up INCLUDE/LIB/PATH before invoking cl.exe. Prefers VsDevCmd.bat, falls
    // back to vcvars64.bat. Returns "" if none is found.
    static std::string FindVsDevCmdScript() {
        const char* programFilesEnv[] = {"ProgramFiles", "ProgramFiles(x86)"};
        const std::vector<std::string> years = {"2022", "2019", "2017"};
        const std::vector<std::string> editions = {"Enterprise", "Professional", "Community", "BuildTools"};

        for (const char* envVar : programFilesEnv) {
            char pf[MAX_PATH];
            if (GetEnvironmentVariableA(envVar, pf, MAX_PATH) == 0) {
                continue;
            }
            fs::path base = fs::path(pf) / "Microsoft Visual Studio";
            for (const auto& year : years) {
                for (const auto& ed : editions) {
                    fs::path root = base / year / ed;
                    if (!fs::exists(root) || !fs::is_directory(root)) {
                        continue;
                    }
                    fs::path vsdev = root / "Common7/Tools/VsDevCmd.bat";
                    if (fs::exists(vsdev)) return vsdev.string();
                    fs::path vcvars = root / "VC/Auxiliary/Build/vcvars64.bat";
                    if (fs::exists(vcvars)) return vcvars.string();
                }
            }
        }
        return "";
    }

private:
    static std::string FindInPath(const std::string& exeName) {
        std::string pathValue = ReadEnvironmentVariableString("PATH");
        if (pathValue.empty()) return "";

        std::stringstream ss(pathValue);
        std::string item;
        while (std::getline(ss, item, ';')) {
            fs::path p = fs::path(item) / exeName;
            if (fs::exists(p)) {
                return p.string();
            }
        }
        return "";
    }

    static std::string SearchVisualStudio() {
        std::vector<std::string> dirs = GetVisualStudioBuildToolDirs();
        for (const auto& dir : dirs) {
            fs::path clPath = fs::path(dir) / "cl.exe";
            if (fs::exists(clPath)) {
                return clPath.string();
            }
        }
        return "";
    }
};

// ============================================================================
// SANDBOXED SECURITY EXECUTION ENGINE
// ============================================================================

