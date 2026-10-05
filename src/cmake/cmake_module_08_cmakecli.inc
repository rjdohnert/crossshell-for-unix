class CMakeCLI {
public:
    static void PrintVersion() {
        std::cout << "cmake version 6.30.0 \n"
                  << "Licensed under the terms of the BSD 3-Clause License.\n";
    }

    static void PrintHelp() {
        std::cout << R"HELP(cmake(1)                CrossShell for UNIX Reference Manual                 cmake(1)

    NAME
        cmake - Configure, build, install, and run CMake projects on Windows.

    SYNOPSIS
        cmake [OPTIONS] SOURCE
        cmake [OPTIONS] -S SOURCE -B BUILD
        cmake --build BUILD [BUILD_OPTIONS] [-- NATIVE_OPTIONS...]
        cmake --install BUILD
        cmake -P SCRIPT

    DESCRIPTION
        Provides a Windows-native CMake-compatible command-line interface.
        Configuration mode generates Visual Studio, Ninja, NMake, or MinGW
        build files. Build and install modes launch the selected native tools
        as child processes managed by a Windows job object.

    OPTIONS
        -S SOURCE
            Explicitly specify the source directory.

        -B BUILD
            Explicitly specify the build directory.

        -C FILE
            Preload a script that populates the cache.

        -D NAME[:TYPE]=VALUE
            Create or update a cache entry.

        -U GLOB
            Remove matching entries from the CMake cache.

        -G GENERATOR
            Select a build-system generator.

        -T TOOLSET
            Select a generator toolset.

        -A PLATFORM
            Select a generator platform.

        --fresh
            Configure a fresh build tree and remove the existing cache.

        -h, -H, --help, -?, /?
            Display this reference manual.

        --version, -version
            Display version and license information.

    BUILD OPTIONS
        --target TARGET, -t TARGET
            Build TARGET instead of the default target.

        --config CONFIG
            Select a configuration for a multi-configuration generator.

        --clean-first
            Build the clean target before the requested target.

        --parallel [JOBS], -j [JOBS]
            Build in parallel, optionally using JOBS workers.

        --verbose, -v
            Enable verbose native build output.

        -- NATIVE_OPTIONS...
            Pass the remaining options to the native build tool.

    GENERATORS
        Visual Studio 17 2022, Visual Studio 16 2019
            Generate Visual Studio solution and project files.

        Ninja, Ninja Multi-Config
            Generate Ninja build files.

        NMake Makefiles, MinGW Makefiles
            Generate makefiles for the corresponding Windows toolchain.

    EXAMPLES
        cmake -S . -B build -G "Visual Studio 17 2022"
            Configure the current source tree in the build directory.

        cmake --build build --config Release --parallel 8
            Build the Release configuration using eight workers.

        cmake --install build
            Install the project from the build directory.

        cmake -P configure.cmake
            Execute a CMake script.

    CrossShell for UNIX                                                        cmake(1)
)HELP";
    }

    static bool Parse(int argc, wchar_t* argv[], Options& opt) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = StringUtil::WideToUtf8(argv[i]);

            if (arg == "--version" || arg == "-version") {
                opt.mode = Mode::Version;
                return true;
            }
            if (arg == "-h" || arg == "-H" || arg == "--help" || arg == "-?" || arg == "/?") {
                opt.mode = Mode::Help;
                return true;
            }
            if (arg == "--build") {
                opt.mode = Mode::Build;
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    opt.buildDir = fs::path(argv[++i]).make_preferred();
                }
                continue;
            }
            if (arg == "--install") {
                opt.mode = Mode::Install;
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    opt.buildDir = fs::path(argv[++i]).make_preferred();
                }
                continue;
            }
            if (arg == "-P") {
                opt.mode = Mode::Script;
                if (i + 1 < argc) {
                    opt.scriptPath = fs::path(argv[++i]).make_preferred();
                }
                continue;
            }

            // Build Options
            if (arg == "--target" || arg == "-t") {
                if (i + 1 < argc) opt.target = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg == "--config") {
                if (i + 1 < argc) opt.config = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg == "--clean-first") {
                opt.cleanFirst = true;
                continue;
            }
            if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
                continue;
            }
            if (arg == "-j" || arg == "--parallel") {
                if (i + 1 < argc && iswdigit(argv[i + 1][0])) {
                    opt.parallelJobs = std::stoi(argv[++i]);
                } else {
                    opt.parallelJobs = static_cast<int>(std::thread::hardware_concurrency());
                }
                continue;
            }
            if (arg == "--") {
                for (int k = i + 1; k < argc; ++k) {
                    opt.nativeToolArgs.push_back(StringUtil::WideToUtf8(argv[k]));
                }
                break;
            }

            // Generation Options
            if (arg == "-S") {
                if (i + 1 < argc) opt.sourceDir = fs::path(argv[++i]).make_preferred();
                continue;
            }
            if (arg == "-B") {
                if (i + 1 < argc) opt.binaryDir = fs::path(argv[++i]).make_preferred();
                continue;
            }
            if (arg.rfind("-B", 0) == 0 && arg.length() > 2) {
                opt.binaryDir = fs::path(StringUtil::Utf8ToWide(arg.substr(2))).make_preferred();
                continue;
            }
            if (arg.rfind("-S", 0) == 0 && arg.length() > 2) {
                opt.sourceDir = fs::path(StringUtil::Utf8ToWide(arg.substr(2))).make_preferred();
                continue;
            }
            if (arg == "-G") {
                if (i + 1 < argc) opt.generator = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg == "-A") {
                if (i + 1 < argc) opt.architecture = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg == "-T") {
                if (i + 1 < argc) opt.toolset = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg == "--fresh") {
                opt.fresh = true;
                continue;
            }
            if (arg == "-C") {
                if (i + 1 < argc) opt.initialCacheScript = StringUtil::WideToUtf8(argv[++i]);
                continue;
            }
            if (arg.rfind("-D", 0) == 0) {
                std::string definition = arg.substr(2);
                if (definition.empty() && i + 1 < argc) {
                    definition = StringUtil::WideToUtf8(argv[++i]);
                }
                ParseDefine(definition, opt);
                continue;
            }
            if (arg.rfind("-U", 0) == 0) {
                std::string undef = arg.substr(2);
                if (undef.empty() && i + 1 < argc) undef = StringUtil::WideToUtf8(argv[++i]);
                opt.cacheUndefines.push_back(undef);
                continue;
            }

            // Positional Directories
            if (!arg.empty() && arg[0] != '-') {
                fs::path p = fs::path(argv[i]).make_preferred();
                std::error_code ec;
                if (fs::exists(p / "CMakeLists.txt", ec)) {
                    opt.sourceDir = p;
                } else {
                    opt.binaryDir = p;
                }
            }
        }
        return true;
    }

private:
    static void ParseDefine(const std::string& def, Options& opt) {
        size_t eqPos = def.find('=');
        if (eqPos == std::string::npos) {
            opt.cacheDefines[def] = {"ON", "BOOL", ""};
            return;
        }

        std::string keyType = def.substr(0, eqPos);
        std::string value = def.substr(eqPos + 1);
        std::string name = keyType;
        std::string type = "UNINITIALIZED";

        size_t colonPos = keyType.find(':');
        if (colonPos != std::string::npos) {
            name = keyType.substr(0, colonPos);
            type = keyType.substr(colonPos + 1);
        }

        opt.cacheDefines[name] = {value, type, ""};
    }
};

// ============================================================================
// Core CMake Engine
// ============================================================================
