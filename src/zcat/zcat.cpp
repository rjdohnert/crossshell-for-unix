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
#include <sstream>
#include <io.h>
#include <streambuf>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };

// ============================================================================
// 1. PIPELINE BUFFER & FORMATTING UTILITIES
// ============================================================================
class PipeBuffer : public std::streambuf {
private:
    FILE* m_file;
    char m_buffer[4096];

public:
    explicit PipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + sizeof(m_buffer));
    }

    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    int sync() override {
        auto n = pptr() - pbase();
        if (n > 0) {
            std::fwrite(pbase(), 1, static_cast<size_t>(n), m_file);
        }
        setp(m_buffer, m_buffer + sizeof(m_buffer));
        return std::fflush(m_file);
    }
};

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
    static std::string QuoteJson(const std::string& value) {
        std::string out = "\"";
        for (unsigned char ch : value) {
            if (ch == '"' || ch == '\\') out += '\\';
            if (ch == '\n') out += "\\n";
            else if (ch == '\r') out += "\\r";
            else if (ch == '\t') out += "\\t";
            else if (ch < 0x20) out += '?';
            else out += static_cast<char>(ch);
        }
        return out + "\"";
    }

    static std::string QuoteCsv(const std::string& value) {
        std::string out = "\"";
        for (char ch : value) out += (ch == '"') ? "\"\"" : std::string(1, ch);
        return out + "\"";
    }

    static void EmitData(const std::string& data, OutputFormat format, std::ostream& out) {
        if (format == OutputFormat::Json) {
            out << "{\"content\":" << QuoteJson(data) << "}\n";
        } else if (format == OutputFormat::Csv) {
            out << "\"content\"\n" << QuoteCsv(data) << "\n";
        } else if (format == OutputFormat::Table) {
            out << "CONTENT\n-------\n" << data << (data.empty() || data.back() == '\n' ? "" : "\n");
        }
    }
};

// ============================================================================
// 2. PROCESS RUNNER & GZIP STREAMER
// ============================================================================
class ProcessRunner {
public:
    int RunPowerShell(const std::wstring& script) const {
        std::vector<std::wstring> args = {
            L"powershell",
            L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-Command",
            script
        };

        std::wstring cmdLine = CommandLineFormatter::BuildCommandLine(args);
        std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
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

class GzipStreamer {
private:
    ProcessRunner m_runner;

public:
    explicit GzipStreamer(const ProcessRunner& runner) : m_runner(runner) {}

    int EmitGzipFile(const fs::path& inputPath, OutputFormat format) const {
        std::wstring inEsc = CommandLineFormatter::EscapeSingleQuoted(inputPath.wstring());
        std::wstring outputPath;
        if (format != OutputFormat::Human) {
            wchar_t tempDir[MAX_PATH] = {}, tempFile[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            GetTempFileNameW(tempDir, L"zca", 0, tempFile);
            outputPath = tempFile;
        }

        std::wstring script =
            L"$in='" + inEsc + L"';"
            L"$fi=[System.IO.File]::OpenRead($in);"
            L"try{"
            L"$gz=New-Object System.IO.Compression.GzipStream($fi,[System.IO.Compression.CompressionMode]::Decompress);"
            + (format == OutputFormat::Human ? L"try{$gz.CopyTo([Console]::OpenStandardOutput())}finally{$gz.Dispose()}" : L"try{$fo=[IO.File]::Create('" + CommandLineFormatter::EscapeSingleQuoted(outputPath) + L"');try{$gz.CopyTo($fo)}finally{$fo.Dispose()};}finally{$gz.Dispose()}")
            + L"}finally{$fi.Dispose()}";

        int rc = m_runner.RunPowerShell(script);
        if (rc == 0 && format != OutputFormat::Human) {
            std::ifstream file(outputPath, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            DeleteFileW(outputPath.c_str());
            OutputFormatter::EmitData(data, format, std::cout);
        }
        return rc;
    }
};

// ============================================================================
// 3. OPTIONS & CLI PARSER
// ============================================================================
struct ZcatOptions {
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> files;
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" file.gz...\n"
            << L"Write decompressed gzip file content to standard output.\n\n"
            << L"Options:\n"
            << L"      --json         Emit decompressed content as JSON\n"
            << L"      --csv          Emit decompressed content as CSV\n"
            << L"      --table        Emit decompressed content as a table\n"
            << L"      --pipe COMMAND Send output through COMMAND\n"
            << L"  -h, --help     Show this help\n"
            << L"      --version  Show version\n";
    }

    static void PrintVersion() {
        std::wcout << L"zcat 1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], ZcatOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i] ? argv[i] : L"";
            } else if (arg == L"-h" || arg == L"--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"--version") {
                opts.showVersion = true;
                return true;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"zcat: unknown option -- " << arg << L"\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }
        return true;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================
class ZcatApplication {
private:
    OptionParser m_parser;
    ProcessRunner m_runner;
    GzipStreamer m_streamer;

public:
    ZcatApplication() : m_streamer(m_runner) {}

    int Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"zcat";
        if (argc <= 1) {
            OptionParser::PrintUsage(progName);
            return 1;
        }

        ZcatOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 2;
        }

        if (opts.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }
        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        FILE* pipe = nullptr;
        std::streambuf* oldOutput = nullptr;
        PipeBuffer* pipeBuffer = nullptr;
        if (!opts.pipeCommand.empty()) {
            pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (pipe) {
                oldOutput = std::cout.rdbuf();
                pipeBuffer = new PipeBuffer(pipe);
                std::cout.rdbuf(pipeBuffer);
            }
        }

        int overall = 0;
        for (const std::wstring& f : opts.files) {
            fs::path inPath(f);
            std::error_code ec;
            if (!fs::exists(inPath, ec) || fs::is_directory(inPath, ec)) {
                std::wcerr << L"zcat: cannot access: " << f << L"\n";
                overall = 1;
                continue;
            }

            int rc = m_streamer.EmitGzipFile(inPath, opts.format);
            if (rc != 0) {
                std::wcerr << L"zcat: failed processing: " << f << L"\n";
                overall = 1;
            }
        }

        if (pipeBuffer) {
            std::cout.flush();
            std::cout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(pipe);
        }

        return overall;
    }
};

// ============================================================================
// 5. ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    ZcatApplication app;
    return app.Run(argc, argv);
}

