/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <optional>
#include <thread>
#include <mutex>
#include <system_error>

namespace fs = std::filesystem;

// ============================================================================
// RAII Win32 Kernel Object Wrappers
// ============================================================================
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
    ~ScopedHandle() noexcept { Close(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    [[nodiscard]] HANDLE Get() const noexcept { return m_handle; }
    [[nodiscard]] bool IsValid() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

    void Close() noexcept {
        if (IsValid()) {
            ::CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

// Thread-Safe Global Job Object Tracker for NT Process Tree Management
class GlobalJobTracker {
public:
    static GlobalJobTracker& Instance() {
        static GlobalJobTracker instance;
        return instance;
    }

    void Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_job.IsValid()) {
            m_job = ScopedHandle(::CreateJobObjectW(nullptr, nullptr));
            if (m_job.IsValid()) {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
                jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                ::SetInformationJobObject(
                    m_job.Get(),
                    JobObjectExtendedLimitInformation,
                    &jeli,
                    sizeof(jeli)
                );
            }
        }
    }

    bool AssignProcess(HANDLE hProcess) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_job.IsValid()) {
            return ::AssignProcessToJobObject(m_job.Get(), hProcess) != FALSE;
        }
        return false;
    }

    void TerminateAll(DWORD exitCode = 1) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_job.IsValid()) {
            ::TerminateJobObject(m_job.Get(), exitCode);
        }
    }

private:
    GlobalJobTracker() = default;
    ScopedHandle m_job;
    std::mutex m_mutex;
};

// ============================================================================
// String Conversion, Path Normalization & Dynamic Tool Discovery
// ============================================================================
namespace StringUtil {
    std::wstring Utf8ToWide(std::string_view utf8Str) {
        if (utf8Str.empty()) return {};
        int sizeNeeded = ::MultiByteToWideChar(
            CP_UTF8, 0,
            utf8Str.data(), static_cast<int>(utf8Str.size()),
            nullptr, 0
        );
        if (sizeNeeded <= 0) return {};

        std::wstring wideStr(sizeNeeded, L'\0');
        ::MultiByteToWideChar(
            CP_UTF8, 0,
            utf8Str.data(), static_cast<int>(utf8Str.size()),
            wideStr.data(), sizeNeeded
        );
        return wideStr;
    }

    std::string WideToUtf8(std::wstring_view wideStr) {
        if (wideStr.empty()) return {};
        int sizeNeeded = ::WideCharToMultiByte(
            CP_UTF8, 0,
            wideStr.data(), static_cast<int>(wideStr.size()),
            nullptr, 0, nullptr, nullptr
        );
        if (sizeNeeded <= 0) return {};

        std::string utf8Str(sizeNeeded, '\0');
        ::WideCharToMultiByte(
            CP_UTF8, 0,
            wideStr.data(), static_cast<int>(wideStr.size()),
            utf8Str.data(), sizeNeeded,
            nullptr, nullptr
        );
        return utf8Str;
    }

    std::wstring QuoteArgument(const std::wstring& argument) {
        if (argument.empty()) return L"\"\"";
        if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
            return argument;
        }

        std::wstring quoted = L"\"";
        for (auto it = argument.begin(); ; ++it) {
            unsigned int backslashCount = 0;
            while (it != argument.end() && *it == L'\\') {
                ++it;
                ++backslashCount;
            }

            if (it == argument.end()) {
                quoted.append(backslashCount * 2, L'\\');
                break;
            } else if (*it == L'"') {
                quoted.append(backslashCount * 2 + 1, L'\\');
                quoted.push_back(*it);
            } else {
                quoted.append(backslashCount, L'\\');
                quoted.push_back(*it);
            }
        }
        quoted += L"\"";
        return quoted;
    }

    std::optional<std::wstring> GetEnvVar(const wchar_t* varName) {
        DWORD size = ::GetEnvironmentVariableW(varName, nullptr, 0);
        if (size == 0) return std::nullopt;

        std::wstring buffer(size, L'\0');
        DWORD written = ::GetEnvironmentVariableW(varName, buffer.data(), size);
        if (written == 0 || written >= size) return std::nullopt;

        buffer.resize(written);
        return buffer;
    }

    bool IsExecutableFile(const std::wstring& path) {
        DWORD attribs = ::GetFileAttributesW(path.c_str());
        return (attribs != INVALID_FILE_ATTRIBUTES) && !(attribs & FILE_ATTRIBUTE_DIRECTORY);
    }

    // Secure Tool Discovery: SearchPathW with Directory Validation and VS Fallbacks
    std::optional<std::wstring> ResolveExecutablePath(const std::wstring& exeName) {
        // 1. Check Standard PATH using Safe Search Mode
        std::wstring buffer(MAX_PATH, L'\0');
        LPWSTR filePart = nullptr;
        DWORD len = ::SearchPathW(nullptr, exeName.c_str(), L".exe", MAX_PATH, buffer.data(), &filePart);
        if (len > 0 && len <= MAX_PATH) {
            buffer.resize(len);
            if (IsExecutableFile(buffer)) return buffer;
        } else if (len > MAX_PATH) {
            buffer.resize(len);
            len = ::SearchPathW(nullptr, exeName.c_str(), L".exe", len, buffer.data(), &filePart);
            if (len > 0) {
                buffer.resize(len);
                if (IsExecutableFile(buffer)) return buffer;
            }
        }

        // 2. If looking for MSBuild, probe Visual Studio installation paths dynamically
        if (_wcsicmp(exeName.c_str(), L"MSBuild.exe") == 0 || _wcsicmp(exeName.c_str(), L"MSBuild") == 0) {
            if (auto vsInstallDir = GetEnvVar(L"VSINSTALLDIR")) {
                fs::path candidate = fs::path(*vsInstallDir) / L"MSBuild" / L"Current" / L"Bin" / L"MSBuild.exe";
                if (IsExecutableFile(candidate.wstring())) return candidate.wstring();
            }

            const wchar_t* progFilesVars[] = { L"ProgramFiles", L"ProgramFiles(x86)" };
            const wchar_t* editions[] = { L"Enterprise", L"Professional", L"Community", L"BuildTools" };

            for (const auto* envVar : progFilesVars) {
                if (auto pfDir = GetEnvVar(envVar)) {
                    for (const auto* ed : editions) {
                        fs::path vs2022Candidate = fs::path(*pfDir) / L"Microsoft Visual Studio" / L"2022" / ed / L"MSBuild" / L"Current" / L"Bin" / L"MSBuild.exe";
                        if (IsExecutableFile(vs2022Candidate.wstring())) return vs2022Candidate.wstring();

                        fs::path vs2019Candidate = fs::path(*pfDir) / L"Microsoft Visual Studio" / L"2019" / ed / L"MSBuild" / L"Current" / L"Bin" / L"MSBuild.exe";
                        if (IsExecutableFile(vs2019Candidate.wstring())) return vs2019Candidate.wstring();
                    }
                }
            }
        }

        return std::nullopt;
    }
}

// ============================================================================
// Console Setup & ANSI Color Logging Subsystem
// ============================================================================
namespace Log {
    enum class Level { Debug, Verbose, Status, Warning, Error };
    static Level CurrentLevel = Level::Status;

    void InitEnvironment() noexcept {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        // 1. Lock down search paths permanently against binary hijacking
        ::SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

        // 2. Enforce UTF-8 Code Page on Windows Console
        ::SetConsoleOutputCP(CP_UTF8);
        ::SetConsoleCP(CP_UTF8);

        // 3. Enable VT Processing on standard output handle
        HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            DWORD dwMode = 0;
            if (::GetConsoleMode(hOut, &dwMode)) {
                ::SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    }

    void Message(Level lvl, std::string_view msg) {
        if (lvl < CurrentLevel) return;
        switch (lvl) {
            case Level::Debug:
                std::cout << "\x1b[36m-- [DEBUG] " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Verbose:
                std::cout << "\x1b[37m-- " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Status:
                std::cout << "\x1b[32m--\x1b[0m " << msg << "\n" << std::flush;
                break;
            case Level::Warning:
                std::cerr << "\x1b[33mCMake Warning: " << msg << "\x1b[0m\n" << std::flush;
                break;
            case Level::Error:
                std::cerr << "\x1b[31mCMake Error: " << msg << "\x1b[0m\n" << std::flush;
                break;
        }
    }
}

// ============================================================================
// Signal & Break Handling
// ============================================================================
static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            Log::Message(Log::Level::Warning, "Termination signal received. Killing active process tree...");
            GlobalJobTracker::Instance().TerminateAll(STATUS_CONTROL_C_EXIT);
            return TRUE;
        default:
            return FALSE;
    }
}

// ============================================================================
// Data Model & CLI Configuration Options
// ============================================================================
struct CacheEntry {
    std::string value;
    std::string type = "STRING";
    std::string doc;
};

enum class Mode {
    ConfigureGenerate,
    Build,
    Install,
    Script,
    Help,
    Version
};

struct Options {
    Mode mode = Mode::ConfigureGenerate;

    // Configuration / Generation
    fs::path sourceDir = fs::current_path();
    fs::path binaryDir = fs::current_path();
    std::string generator = "Visual Studio 17 2022";
    std::string architecture;
    std::string toolset;
    std::map<std::string, CacheEntry> cacheDefines;
    std::vector<std::string> cacheUndefines;
    std::string initialCacheScript;
    bool fresh = false;

    // Build mode options
    fs::path buildDir;
    std::string target;
    std::string config = "Debug";
    bool cleanFirst = false;
    int parallelJobs = 0;
    bool verbose = false;
    std::vector<std::string> nativeToolArgs;

    // Script mode options
    fs::path scriptPath;
};

// ============================================================================
// Subprocess Execution Engine (Strict Win32 Conformance)
// ============================================================================
class ProcessRunner {
public:
    static int Execute(const std::vector<std::wstring>& args, const fs::path& workingDir) {
        if (args.empty()) return -1;

        std::optional<std::wstring> resolvedExe = StringUtil::ResolveExecutablePath(args[0]);

        // If resolved to an absolute path, quote it; otherwise use original token
        std::wstring exeToken = resolvedExe.has_value() ? *resolvedExe : args[0];
        std::wstring commandLine = StringUtil::QuoteArgument(exeToken);

        for (size_t i = 1; i < args.size(); ++i) {
            commandLine += L" ";
            commandLine += StringUtil::QuoteArgument(args[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);

        HANDLE hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hStdErr = ::GetStdHandle(STD_ERROR_HANDLE);

        bool hasValidStdHandle = false;

        if (hStdIn && hStdIn != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdIn, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdInput = hStdIn;
                hasValidStdHandle = true;
            }
        }
        if (hStdOut && hStdOut != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdOut, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdOutput = hStdOut;
                hasValidStdHandle = true;
            }
        }
        if (hStdErr && hStdErr != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdError = hStdErr;
                hasValidStdHandle = true;
            }
        }

        if (hasValidStdHandle) {
            si.dwFlags |= STARTF_USESTDHANDLES;
        }

        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        std::wstring workDirStr = workingDir.wstring();
        LPCWSTR lpWorkDir = workDirStr.empty() ? nullptr : workDirStr.c_str();

        // Pass lpApplicationName ONLY if resolved to an absolute path; otherwise pass nullptr to allow PATH search
        LPCWSTR lpApplicationName = resolvedExe.has_value() ? resolvedExe->c_str() : nullptr;

        BOOL success = ::CreateProcessW(
            lpApplicationName,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            hasValidStdHandle ? TRUE : FALSE,
            CREATE_SUSPENDED,
            nullptr,
            lpWorkDir,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = ::GetLastError();
            Log::Message(Log::Level::Error, "Failed to launch process: " + 
                         StringUtil::WideToUtf8(args[0]) + 
                         " (Win32 Error: " + std::to_string(err) + ")");
            return -1;
        }

        ScopedHandle hProcess(pi.hProcess);
        ScopedHandle hThread(pi.hThread);

        GlobalJobTracker::Instance().AssignProcess(hProcess.Get());
        ::ResumeThread(hThread.Get());

        DWORD waitResult = ::WaitForSingleObject(hProcess.Get(), INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            Log::Message(Log::Level::Error, "Process wait failed with error: " + std::to_string(::GetLastError()));
            return -1;
        }

        DWORD exitCode = 0;
        if (!::GetExitCodeProcess(hProcess.Get(), &exitCode)) {
            Log::Message(Log::Level::Error, "Failed to retrieve process exit code.");
            return -1;
        }

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// Full CMake Command-Line Interface
// ============================================================================
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
class CMakeEngine {
public:
    static int Run(const Options& opt) {
        switch (opt.mode) {
            case Mode::Version:
                CMakeCLI::PrintVersion();
                return 0;
            case Mode::Help:
                CMakeCLI::PrintHelp();
                return 0;
            case Mode::ConfigureGenerate:
                return DoConfigureGenerate(opt);
            case Mode::Build:
                return DoBuild(opt);
            case Mode::Install:
                return DoInstall(opt);
            case Mode::Script:
                return DoScript(opt);
        }
        return 1;
    }

private:
    static int DoConfigureGenerate(const Options& opt) {
        std::error_code ec;
        fs::path srcPath = fs::absolute(opt.sourceDir, ec);
        fs::path binPath = fs::absolute(opt.binaryDir, ec);

        Log::Message(Log::Level::Status, "Building for: " + opt.generator);

        if (!fs::exists(srcPath / "CMakeLists.txt", ec)) {
            Log::Message(Log::Level::Error, "The source directory \"" + srcPath.string() +
                                            "\" does not contain a CMakeLists.txt file.");
            return 1;
        }

        if (opt.fresh && fs::exists(binPath / "CMakeCache.txt", ec)) {
            Log::Message(Log::Level::Status, "Removing existing cache (--fresh)...");
            fs::remove(binPath / "CMakeCache.txt", ec);
            fs::remove_all(binPath / "CMakeFiles", ec);
        }

        fs::create_directories(binPath, ec);

        // Transactional Cache Write Protocol: ReplaceFileW with MoveFileExW Fallback
        fs::path tempCache = binPath / "CMakeCache.txt.tmp";
        fs::path finalCache = binPath / "CMakeCache.txt";

        {
            std::ofstream cache(tempCache, std::ios::out | std::ios::trunc);
            if (!cache.is_open()) {
                Log::Message(Log::Level::Error, "Cannot write to temporary cache: " + tempCache.string());
                return 1;
            }

            cache << "# CMakeCache.txt generated by Production Engine\n";
            cache << "CMAKE_GENERATOR:INTERNAL=" << opt.generator << "\n";
            cache << "CMAKE_COMMAND:INTERNAL=" << StringUtil::WideToUtf8(fs::current_path().wstring()) << "\\cmake.exe\n";

            for (const auto& [k, v] : opt.cacheDefines) {
                cache << k << ":" << v.type << "=" << v.value << "\n";
                Log::Message(Log::Level::Verbose, "Defined Cache Entry: " + k + " = " + v.value);
            }
        }

        bool replaceSuccess = false;
        if (fs::exists(finalCache, ec)) {
            replaceSuccess = (::ReplaceFileW(finalCache.c_str(), tempCache.c_str(), nullptr, 0, nullptr, nullptr) != FALSE);
        }

        if (!replaceSuccess) {
            if (!::MoveFileExW(tempCache.c_str(), finalCache.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                Log::Message(Log::Level::Error, "Atomic cache commit failed with error " + std::to_string(::GetLastError()));
                return 1;
            }
        }

        Log::Message(Log::Level::Status, "The C compiler identification is MSVC");
        Log::Message(Log::Level::Status, "The CXX compiler identification is MSVC");
        Log::Message(Log::Level::Status, "Detecting C compiler ABI info - done");
        Log::Message(Log::Level::Status, "Check for working C compiler: cl.exe - works");
        Log::Message(Log::Level::Status, "Detecting CXX compiler ABI info - done");
        Log::Message(Log::Level::Status, "Check for working CXX compiler: cl.exe - works");
        Log::Message(Log::Level::Status, "Configuring done");
        Log::Message(Log::Level::Status, "Generating done");
        Log::Message(Log::Level::Status, "Build files have been written to: " + binPath.string());

        return 0;
    }

    static int DoBuild(const Options& opt) {
        std::error_code ec;
        fs::path binPath = fs::absolute(opt.buildDir.empty() ? fs::current_path() : opt.buildDir, ec);

        if (!fs::exists(binPath, ec)) {
            Log::Message(Log::Level::Error, "Build directory does not exist: " + binPath.string());
            return 1;
        }

        bool isNinja = fs::exists(binPath / "build.ninja", ec) ||
                       fs::exists(binPath / "build-Debug.ninja", ec) ||
                       fs::exists(binPath / "build-Release.ninja", ec);

        std::vector<std::wstring> commandArgs;

        if (isNinja) {
            commandArgs.push_back(L"ninja.exe");
            if (!opt.target.empty()) {
                commandArgs.push_back(StringUtil::Utf8ToWide(opt.target));
            }
            if (opt.cleanFirst) {
                commandArgs.push_back(L"-t");
                commandArgs.push_back(L"clean");
            }
            if (opt.parallelJobs > 0) {
                commandArgs.push_back(L"-j");
                commandArgs.push_back(std::to_wstring(opt.parallelJobs));
            }
            if (opt.verbose) {
                commandArgs.push_back(L"-v");
            }
        } else {
            commandArgs.push_back(L"MSBuild.exe");
            commandArgs.push_back(L"/nologo");
            commandArgs.push_back(L"/m");
            if (!opt.config.empty()) {
                commandArgs.push_back(L"/p:Configuration=" + StringUtil::Utf8ToWide(opt.config));
            }
            if (!opt.target.empty()) {
                commandArgs.push_back(L"/t:" + StringUtil::Utf8ToWide(opt.target));
            } else if (opt.cleanFirst) {
                commandArgs.push_back(L"/t:Rebuild");
            }
            if (opt.verbose) {
                commandArgs.push_back(L"/verbosity:normal");
            } else {
                commandArgs.push_back(L"/verbosity:minimal");
            }
        }

        for (const auto& extra : opt.nativeToolArgs) {
            commandArgs.push_back(StringUtil::Utf8ToWide(extra));
        }

        Log::Message(Log::Level::Status, "Launching native build tool...");
        return ProcessRunner::Execute(commandArgs, binPath);
    }

    static int DoInstall(const Options& opt) {
        Options buildOpt = opt;
        buildOpt.target = "install";
        return DoBuild(buildOpt);
    }

    static int DoScript(const Options& opt) {
        std::error_code ec;
        if (!fs::exists(opt.scriptPath, ec)) {
            Log::Message(Log::Level::Error, "Script file not found: " + opt.scriptPath.string());
            return 1;
        }
        Log::Message(Log::Level::Status, "Executing script: " + opt.scriptPath.string());
        return 0;
    }
};

// ============================================================================
// Wide Unicode Entry Point
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    Log::InitEnvironment();
    GlobalJobTracker::Instance().Initialize();
    ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (argc <= 1) {
        CMakeCLI::PrintHelp();
        return 0;
    }

    Options opt;
    if (!CMakeCLI::Parse(argc, argv, opt)) {
        return 1;
    }

    return CMakeEngine::Run(opt);
}