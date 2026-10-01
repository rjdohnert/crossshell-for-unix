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
 *
 * CrossShell for UNIX
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <cstdio>

static std::string env_utf8(const std::wstring& value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// ============================================================================
// Case-Insensitive Comparator for Windows Environment Variables
// ============================================================================

struct CaseInsensitiveWStringCompare {
    bool operator()(const std::wstring& lhs, const std::wstring& rhs) const {
        return _wcsicmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

// ============================================================================
// Data Models & Configuration Options
// ============================================================================

struct ProgramOptions {
    bool ignoreEnvironment = false;        // -i, -, --ignore-environment
    bool nullDelimiter = false;            // -0, --null
    bool verbose = false;                  // -v, --verbose
    bool showHelp = false;                 // -?, --help
    bool showVersion = false;              // --version
    std::wstring changeDir;                // -C, --chdir=DIR
    std::vector<std::wstring> unsetVars;   // -u, --unset=NAME
    std::vector<std::pair<std::wstring, std::wstring>> setVars; // NAME=VALUE
    std::vector<std::wstring> commandAndArgs;                   // COMMAND [ARG]...
    int outputFormat = 0;
    std::wstring pipeCommand;
};

// ============================================================================
// Environment Manager (OOP Environment Map & Serialization)
// ============================================================================

class EnvironmentManager {
public:
    using EnvMap = std::map<std::wstring, std::wstring, CaseInsensitiveWStringCompare>;

    EnvironmentManager() = default;

    void loadSystemEnvironment() {
        m_envMap.clear();
        LPWCH envBlock = GetEnvironmentStringsW();
        if (!envBlock) return;

        LPWCH current = envBlock;
        while (*current) {
            std::wstring entry = current;
            // Skip Windows hidden/per-drive working directory variables (e.g. "=C:=")
            if (!entry.empty() && entry[0] != L'=') {
                size_t equalsPos = entry.find(L'=');
                if (equalsPos != std::wstring::npos) {
                    std::wstring key = entry.substr(0, equalsPos);
                    std::wstring val = entry.substr(equalsPos + 1);
                    m_envMap[key] = val;
                }
            }
            current += entry.length() + 1;
        }

        FreeEnvironmentStringsW(envBlock);
    }

    void clear() {
        m_envMap.clear();
    }

    void set(const std::wstring& name, const std::wstring& value) {
        m_envMap[name] = value;
    }

    void unset(const std::wstring& name) {
        m_envMap.erase(name);
    }

    const EnvMap& getMap() const {
        return m_envMap;
    }

    // Windows CreateProcessW requires a double-null-terminated block of sorted strings:
    // "VAR1=VAL1\0VAR2=VAL2\0\0"
    std::vector<wchar_t> createEnvironmentBlock() const {
        std::vector<wchar_t> block;
        for (const auto& [key, val] : m_envMap) {
            std::wstring entry = key + L"=" + val;
            block.insert(block.end(), entry.begin(), entry.end());
            block.push_back(L'\0');
        }
        block.push_back(L'\0'); // Final terminating null
        return block;
    }

    void printEnvironment(std::wostream& os, wchar_t delimiter) const {
        for (const auto& [key, val] : m_envMap) {
            os << key << L"=" << val << delimiter;
        }
        os.flush();
    }

private:
    EnvMap m_envMap;
};

// ============================================================================
// Command-Line & Argument Tokenizer (-S / --split-string support)
// ============================================================================

class ArgumentTokenizer {
public:
    // Implements POSIX/GNU env -S split-string parsing rules
    static std::vector<std::wstring> splitString(const std::wstring& str) {
        std::vector<std::wstring> tokens;
        std::wstring current;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        bool escaping = false;

        for (size_t i = 0; i < str.length(); ++i) {
            wchar_t ch = str[i];

            if (escaping) {
                switch (ch) {
                    case L'n': current += L'\n'; break;
                    case L't': current += L'\t'; break;
                    case L'r': current += L'\r'; break;
                    case L'\\': current += L'\\'; break;
                    case L'\'': current += L'\''; break;
                    case L'\"': current += L'\"'; break;
                    case L'_': current += L' '; break; // GNU env \_ is space
                    case L'#': current += L'#'; break;
                    default: current += ch; break;
                }
                escaping = false;
                continue;
            }

            if (ch == L'\\' && !inSingleQuote) {
                escaping = true;
                continue;
            }

            if (ch == L'\'' && !inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
                continue;
            }

            if (ch == L'\"' && !inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }

            if (iswspace(ch) && !inSingleQuote && !inDoubleQuote) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                continue;
            }

            current += ch;
        }

        if (!current.empty()) {
            tokens.push_back(current);
        }

        return tokens;
    }

    // Encodes a vector of arguments into a single Win32 CreateProcess command line
    static std::wstring buildWindowsCommandLine(const std::vector<std::wstring>& args) {
        std::wstring result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += L' ';
            result += quoteWindowsArg(args[i]);
        }
        return result;
    }

private:
    static std::wstring quoteWindowsArg(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";

        bool needsQuotes = (arg.find_first_of(L" \t\n\v\"") != std::wstring::npos);
        if (!needsQuotes) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;

        for (wchar_t ch : arg) {
            if (ch == L'\\') {
                backslashes++;
            } else if (ch == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted += L'\"';
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                backslashes = 0;
                quoted += ch;
            }
        }

        quoted.append(backslashes * 2, L'\\');
        quoted += L'\"';
        return quoted;
    }
};

// ============================================================================
// Process Runner & Pipe Relay
// ============================================================================

class ProcessRunner {
public:
    static int execute(
        const std::vector<std::wstring>& commandAndArgs,
        const std::vector<wchar_t>& envBlock,
        const std::wstring& workingDirectory,
        bool verbose) 
    {
        if (commandAndArgs.empty()) return 0;

        std::wstring commandLine = ArgumentTokenizer::buildWindowsCommandLine(commandAndArgs);

        if (verbose) {
            std::wcerr << L"env: executing: " << commandLine << L"\n";
            if (!workingDirectory.empty()) {
                std::wcerr << L"env: working directory: " << workingDirectory << L"\n";
            }
        }

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        // Seamless pipe inheritance: inherit the current process's standard handles
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        // Create mutable command line buffer for CreateProcessW
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        LPCWSTR lpCurrentDir = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
        LPVOID lpEnvironment = envBlock.empty() ? nullptr : (LPVOID)envBlock.data();

        DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

        BOOL success = CreateProcessW(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            TRUE, // Inherit standard pipes and handles
            creationFlags,
            lpEnvironment,
            lpCurrentDir,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            std::wcerr << L"env: '" << commandAndArgs[0] << L"': ";
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                std::wcerr << L"No such file or directory\n";
                return 127; // POSIX standard: 127 for not found
            } else if (err == ERROR_ACCESS_DENIED) {
                std::wcerr << L"Permission denied\n";
                return 126; // POSIX standard: 126 for non-executable
            } else {
                std::wcerr << L"Failed to create process (Error " << err << L")\n";
                return 126;
            }
        }

        // Wait for child process to finish
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// Command Line Parser
// ============================================================================

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]) {
        std::vector<std::wstring> rawArgs;
        for (int i = 1; i < argc; ++i) {
            rawArgs.push_back(argv[i]);
        }
        return parseTokens(rawArgs);
    }

    static ProgramOptions parseTokens(const std::vector<std::wstring>& args) {
        ProgramOptions opts;
        size_t idx = 0;

        while (idx < args.size()) {
            const std::wstring& arg = args[idx];

            if (arg == L"--") {
                idx++;
                break;
            }

            if (arg == L"-") {
                opts.ignoreEnvironment = true;
                idx++;
                continue;
            }

            if (arg == L"-i" || arg == L"--ignore-environment") {
                opts.ignoreEnvironment = true;
                idx++;
                continue;
            }
            if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                opts.outputFormat = arg == L"--json" ? 1 : (arg == L"--csv" ? 2 : 3);
                idx++;
                continue;
            }
            if (arg == L"--pipe" && idx + 1 < args.size()) {
                opts.pipeCommand = args[++idx];
                idx++;
                continue;
            }

            if (arg == L"-0" || arg == L"--null") {
                opts.nullDelimiter = true;
                idx++;
                continue;
            }

            if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
                idx++;
                continue;
            }

            if (arg == L"-?" || arg == L"--help") {
                opts.showHelp = true;
                idx++;
                return opts;
            }

            if (arg == L"--version") {
                opts.showVersion = true;
                idx++;
                return opts;
            }

            if (arg.rfind(L"-u", 0) == 0 || arg.rfind(L"--unset", 0) == 0) {
                std::wstring name;
                if (arg.rfind(L"--unset=", 0) == 0) {
                    name = arg.substr(8);
                } else if (arg == L"-u" || arg == L"--unset") {
                    if (idx + 1 < args.size()) {
                        name = args[++idx];
                    }
                } else if (arg.rfind(L"-u", 0) == 0 && arg.length() > 2) {
                    name = arg.substr(2);
                }
                if (!name.empty()) {
                    opts.unsetVars.push_back(name);
                }
                idx++;
                continue;
            }

            if (arg.rfind(L"-C", 0) == 0 || arg.rfind(L"--chdir", 0) == 0) {
                std::wstring dir;
                if (arg.rfind(L"--chdir=", 0) == 0) {
                    dir = arg.substr(8);
                } else if (arg == L"-C" || arg == L"--chdir") {
                    if (idx + 1 < args.size()) {
                        dir = args[++idx];
                    }
                } else if (arg.rfind(L"-C", 0) == 0 && arg.length() > 2) {
                    dir = arg.substr(2);
                }
                opts.changeDir = dir;
                idx++;
                continue;
            }

            if (arg.rfind(L"-S", 0) == 0 || arg.rfind(L"--split-string", 0) == 0) {
                std::wstring splitStr;
                if (arg.rfind(L"--split-string=", 0) == 0) {
                    splitStr = arg.substr(15);
                } else if (arg == L"-S" || arg == L"--split-string") {
                    if (idx + 1 < args.size()) {
                        splitStr = args[++idx];
                    }
                } else if (arg.rfind(L"-S", 0) == 0 && arg.length() > 2) {
                    splitStr = arg.substr(2);
                }
                
                auto expanded = ArgumentTokenizer::splitString(splitStr);
                std::vector<std::wstring> remaining(args.begin() + idx + 1, args.end());
                expanded.insert(expanded.end(), remaining.begin(), remaining.end());
                
                // Re-parse with expanded tokens
                auto subOpts = parseTokens(expanded);
                if (opts.ignoreEnvironment) subOpts.ignoreEnvironment = true;
                if (opts.nullDelimiter) subOpts.nullDelimiter = true;
                if (opts.verbose) subOpts.verbose = true;
                for (const auto& u : opts.unsetVars) subOpts.unsetVars.push_back(u);
                for (const auto& s : opts.setVars) subOpts.setVars.push_back(s);
                return subOpts;
            }

            // Check for NAME=VALUE definitions
            size_t eqPos = arg.find(L'=');
            if (eqPos != std::wstring::npos && eqPos > 0) {
                opts.setVars.emplace_back(arg.substr(0, eqPos), arg.substr(eqPos + 1));
                idx++;
                continue;
            }

            // Once we encounter a non-option, non-assignment argument, it is the COMMAND
            break;
        }

        while (idx < args.size()) {
            opts.commandAndArgs.push_back(args[idx++]);
        }

        return opts;
    }
};

// ============================================================================
// Core Application Controller
// ============================================================================

class EnvApplication {
public:
    explicit EnvApplication(ProgramOptions options)
        : m_opts(std::move(options)) {}

    int run() {
        if (m_opts.showHelp) {
            printHelp();
            return 0;
        }

        if (m_opts.showVersion) {
            printVersion();
            return 0;
        }

        EnvironmentManager envMgr;

        if (!m_opts.ignoreEnvironment) {
            envMgr.loadSystemEnvironment();
        }

        for (const auto& unsetVar : m_opts.unsetVars) {
            if (m_opts.verbose) {
                std::wcerr << L"env: unset: " << unsetVar << L"\n";
            }
            envMgr.unset(unsetVar);
        }

        for (const auto& [key, val] : m_opts.setVars) {
            if (m_opts.verbose) {
                std::wcerr << L"env: set: " << key << L"=" << val << L"\n";
            }
            envMgr.set(key, val);
        }

        // If no command is provided, print the environment to stdout
        if (m_opts.commandAndArgs.empty()) {
            wchar_t delimiter = m_opts.nullDelimiter ? L'\0' : L'\n';
            std::wostringstream captured;
            envMgr.printEnvironment(captured, delimiter);
            std::wstring data = captured.str();
            std::wstring text = m_opts.outputFormat == 1 ? L"{\"environment\":\"" + data + L"\"}\n" : m_opts.outputFormat == 2 ? L"\"environment\"\n\"" + data + L"\"\n" : L"ENVIRONMENT\n-----------\n" + data;
            if (!m_opts.pipeCommand.empty()) { FILE* pipe = _wpopen(m_opts.pipeCommand.c_str(), L"w"); if (!pipe) return 1; std::string narrow = env_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); }
            else if (m_opts.outputFormat) std::wcout << text;
            else std::wcout << data;
            return 0;
        }

        // Execute command
        auto envBlock = envMgr.createEnvironmentBlock();
        return ProcessRunner::execute(
            m_opts.commandAndArgs,
            envBlock,
            m_opts.changeDir,
            m_opts.verbose
        );
    }

private:
    ProgramOptions m_opts;

    static void printVersion() {
        std::wcout << L"env (Windows Native Coreutils) 2.0.0 (x86_64-pc-windows-msvc)\n";
        std::wcout << L"C++17 Object-Oriented Process & Environment Utility\n";
        std::wcout << L"Full pipeline support for Windows CMD, PowerShell, and Unix emulators.\n";
    }

    static void printHelp() {
        std::wcout << LR"(Usage: env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]
Set each NAME to VALUE in the environment and run COMMAND.

Mandatory arguments to long options are mandatory for short options too.
  -i, -, --ignore-environment  start with an empty environment
  -0, --null                   end each output line with NUL, not newline
  -u, --unset=NAME             remove variable from the environment
  -C, --chdir=DIR              change working directory to DIR before execution
  -S, --split-string=S         process and split S into separate arguments;
                                 used for passing multiple arguments in shebangs
  -v, --verbose                print verbose diagnostics for each operation
    --json                       output environment as JSON
    --csv                        output environment as CSV
    --table                      output environment as a table
    --pipe COMMAND               send environment output through COMMAND
  -?, --help                   display this comprehensive help manual and exit
      --version                output version information and exit

A mere - implies -i. If no COMMAND is specified, the resulting environment
is printed to standard output.

Piping and Interoperability:
  - Standard Handles: When running COMMAND, standard input, standard output,
    and standard error are directly inherited. This allows full bidirectional
    piping (e.g. 'type file.txt | env VAR=1 cmd | findstr ...').
  - Null Delimiter: When dumping variables without a command, the -0 / --null flag
    outputs NUL-terminated key=value pairs, enabling safe piping into tools
    like xargs -0 or PowerShell splits without whitespace collisions.
  - Case-Insensitivity: Matches Windows environment semantics (variable lookup
    is case-preserving but case-insensitive).

Exit Status:
  0    if COMMAND was not specified and the environment was printed successfully
  126  if COMMAND was found but could not be invoked (e.g. permission denied)
  127  if COMMAND could not be found
  N    the exit status of COMMAND otherwise

Examples:
  1. Print current environment:
     env

  2. Print environment terminated by null characters (for piping):
     env -0

  3. Run a program with an empty environment:
     env -i powershell.exe

  4. Run a program with an altered variable:
     env NODE_ENV=production node app.js

  5. Unset variables and change working directory:
     env -u TEMP -u TMP -C C:\Projects\Build my_build_tool.exe --release

  6. Pipe data through a modified environment:
     echo test | env LOG_LEVEL=DEBUG findstr "test"

  7. Split-string parameter passing:
     env -S"FOO=BAR DEBUG=1" cmd.exe /c set
)";
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int wmain(int argc, wchar_t* argv[]) {
    // Preserve Unicode output capabilities for Windows pipes and console
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    try {
        ProgramOptions options = CommandLineParser::parse(argc, argv);
        for (int i = 1; i < argc; ++i) { std::wstring arg = argv[i]; if (arg == L"--json") options.outputFormat = 1; else if (arg == L"--csv") options.outputFormat = 2; else if (arg == L"--table") options.outputFormat = 3; else if (arg == L"--pipe" && i + 1 < argc) options.pipeCommand = argv[++i]; }
        EnvApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::wcerr << L"env: fatal error: " << ex.what() << L"\n";
        return 1;
    } catch (...) {
        std::wcerr << L"env: unknown fatal error occurred.\n";
        return 1;
    }
}