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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };
// ============================================================================
// 1. FORMATTING & COMMAND LINE UTILITIES
// ============================================================================
class CommandLineFormatter {
public:
    static std::wstring QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring out;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) out.push_back(L' ');
            out += QuoteArgument(args[i]);
        }
        return out;
    }

    static std::wstring EscapeSingleQuoted(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            if (c == L'\'') out += L"''";
            else out.push_back(c);
        }
        return out;
    }
};

class OutputFormatter {
public:
    static std::string JsonQuote(const std::string& value) {
        std::string out = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c < 0x20 ? '?' : static_cast<char>(c);
        }
        return out + "\"";
    }

    static std::string CsvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) out += c == '"' ? "\"\"" : std::string(1, c);
        return out + "\"";
    }

    static void EmitRecords(std::istream& input, OutputFormat format, FILE* pipe) {
        if (format == OutputFormat::Json && !pipe) std::cout << "[\n";
        bool first = true;
        auto emit = [&](const std::string& text) {
            if (pipe) std::fwrite(text.data(), 1, text.size(), pipe);
            else std::cout << text;
        };

        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (format == OutputFormat::Json) {
                if (!first) emit(",\n");
                first = false;
                emit("{\"match\":" + JsonQuote(line) + "}");
            } else if (format == OutputFormat::Csv) {
                emit(CsvQuote(line) + "\n");
            } else if (format == OutputFormat::Table) {
                emit(line + "\n");
            } else {
                emit(line + "\n");
            }
        }
        if (format == OutputFormat::Json) emit("\n]\n");
    }
};

// ============================================================================
// 2. PROCESS RUNNER & DECOMPRESSOR
// ============================================================================
class ProcessRunner {
public:
    int Run(const wchar_t* app, const std::vector<std::wstring>& args, HANDLE output = INVALID_HANDLE_VALUE) const {
        std::wstring cmdLine = CommandLineFormatter::BuildCommandLine(args);
        std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = output == INVALID_HANDLE_VALUE ? GetStdHandle(STD_OUTPUT_HANDLE) : output;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(app, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
            return 126;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
};

class GzipDecompressor {
private:
    ProcessRunner m_runner;

public:
    explicit GzipDecompressor(const ProcessRunner& runner) : m_runner(runner) {}

    bool Decompress(const fs::path& inputPath, const fs::path& outputPath) const {
        std::wstring inEsc = CommandLineFormatter::EscapeSingleQuoted(inputPath.wstring());
        std::wstring outEsc = CommandLineFormatter::EscapeSingleQuoted(outputPath.wstring());

        std::wstring script =
            L"$in='" + inEsc + L"';"
            L"$out='" + outEsc + L"';"
            L"$fi=[System.IO.File]::OpenRead($in);"
            L"try{"
            L"$gz=New-Object System.IO.Compression.GzipStream($fi,[System.IO.Compression.CompressionMode]::Decompress);"
            L"try{"
            L"$fo=[System.IO.File]::Create($out);"
            L"try{$gz.CopyTo($fo)}finally{$fo.Dispose()}"
            L"}finally{$gz.Dispose()}"
            L"}finally{$fi.Dispose()}";

        std::vector<std::wstring> psArgs = {
            L"powershell",
            L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-Command",
            script
        };

        return m_runner.Run(nullptr, psArgs) == 0;
    }
};

// ============================================================================
// 3. GREP BACKEND LOCATOR
// ============================================================================
class GrepBackendLocator {
public:
    static std::wstring GetModuleDirectory() {
        wchar_t path[MAX_PATH] = {};
        DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return L"";

        std::wstring full(path, len);
        size_t slash = full.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return L"";
        return full.substr(0, slash);
    }

    std::wstring Resolve() const {
        std::wstring local = GetModuleDirectory();
        if (!local.empty()) {
            local += L"\\grep.exe";
            DWORD attrs = GetFileAttributesW(local.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return local;
            }
        }
        return L"grep.exe";
    }
};

// ============================================================================
// 4. OPTIONS & CLI PARSER
// ============================================================================
struct ZgrepOptions {
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> rawArgs;
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [grep-options] pattern file.gz...\n"
            << L"Search compressed gzip files using grep semantics (baseline mode).\n\n"
            << L"Notes:\n"
            << L"  - Gzip files are decompressed to temporary files before grep runs.\n"
            << L"  - stdin mode is not supported in this baseline.\n"
            << L"  - --json, --csv, and --table format grep output records.\n"
            << L"  - --pipe COMMAND sends output through COMMAND.\n"
            << L"  -h, --help     Show this help\n"
            << L"      --version  Show version\n";
    }

    static void PrintVersion() {
        std::wcout << L"zgrep 1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], ZgrepOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i] ? argv[i] : L"";
            if (a == L"-h" || a == L"--help") {
                opts.showHelp = true;
                return true;
            }
            if (a == L"--version") {
                opts.showVersion = true;
                return true;
            }
            if (a == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (a == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (a == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i] ? argv[i] : L"";
            } else {
                opts.rawArgs.push_back(a);
            }
        }
        return true;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================
class ZgrepApplication {
private:
    OptionParser m_parser;
    ProcessRunner m_runner;
    GzipDecompressor m_decompressor;
    GrepBackendLocator m_backendLocator;

public:
    ZgrepApplication() : m_decompressor(m_runner) {}

    int Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"zgrep";
        if (argc <= 1) {
            OptionParser::PrintUsage(progName);
            return 1;
        }

        ZgrepOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }
        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        wchar_t tempPathBuf[MAX_PATH] = {};
        DWORD tp = GetTempPathW(MAX_PATH, tempPathBuf);
        if (tp == 0 || tp >= MAX_PATH) {
            std::wcerr << L"zgrep: failed to get temporary path\n";
            return 1;
        }

        wchar_t tempNameBuf[MAX_PATH] = {};
        if (GetTempFileNameW(tempPathBuf, L"zgr", 0, tempNameBuf) == 0) {
            std::wcerr << L"zgrep: failed to create temporary directory\n";
            return 1;
        }
        fs::path tempRoot = tempNameBuf;
        std::error_code ec;
        fs::remove(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec) {
            std::wcerr << L"zgrep: failed to create temporary directory\n";
            return 1;
        }

        std::vector<fs::path> created;
        std::vector<std::wstring> grepArgs;
        grepArgs.reserve(opts.rawArgs.size());

        int overall = 0;
        int outIndex = 0;

        for (const auto& a : opts.rawArgs) {
            if (a == L"-") {
                std::wcerr << L"zgrep: stdin mode is not supported in this baseline build\n";
                overall = 1;
                continue;
            }

            if (!a.empty() && a[0] == L'-') {
                grepArgs.push_back(a);
                continue;
            }

            fs::path p = a;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
                std::wstring w = p.wstring();
                bool isGz = w.size() >= 3 && _wcsicmp(w.c_str() + (w.size() - 3), L".gz") == 0;
                if (isGz) {
                    fs::path out = tempRoot / (L"zgrep_" + std::to_wstring(outIndex++) + L".txt");
                    if (!m_decompressor.Decompress(p, out)) {
                        std::wcerr << L"zgrep: failed to decompress: " << p.wstring() << L"\n";
                        overall = 1;
                        continue;
                    }
                    created.push_back(out);
                    grepArgs.push_back(out.wstring());
                    continue;
                }
            }

            grepArgs.push_back(a);
        }

        if (grepArgs.empty()) {
            fs::remove_all(tempRoot, ec);
            return (overall == 0) ? 1 : overall;
        }

        std::wstring grepBackend = m_backendLocator.Resolve();
        std::vector<std::wstring> invoke;
        invoke.reserve(grepArgs.size() + 1);
        invoke.push_back(grepBackend);
        for (const auto& a : grepArgs) invoke.push_back(a);

        HANDLE capture = INVALID_HANDLE_VALUE;
        std::wstring capturePath;
        if (opts.format != OutputFormat::Human || !opts.pipeCommand.empty()) {
            wchar_t tempFile[MAX_PATH] = {};
            GetTempFileNameW(tempPathBuf, L"zgo", 0, tempFile);
            capturePath = tempFile;
            capture = CreateFileW(capturePath.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (capture != INVALID_HANDLE_VALUE) SetHandleInformation(capture, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }

        int grepExit = m_runner.Run(grepBackend.c_str(), invoke, capture);

        if (capture != INVALID_HANDLE_VALUE) {
            CloseHandle(capture);
            std::ifstream input(capturePath);
            FILE* pipe = opts.pipeCommand.empty() ? nullptr : _wpopen(opts.pipeCommand.c_str(), L"w");
            OutputFormatter::EmitRecords(input, opts.format, pipe);
            input.close();
            DeleteFileW(capturePath.c_str());
            if (pipe) _pclose(pipe);
        }

        fs::remove_all(tempRoot, ec);
        return (overall != 0) ? overall : grepExit;
    }
};

// ============================================================================
// 6. ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    ZgrepApplication app;
    return app.Run(argc, argv);
}

