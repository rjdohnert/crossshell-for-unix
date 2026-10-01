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
#include <windows.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. DATA MODELS & RAII HANDLERS
// ============================================================================

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = nullptr) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class MailOptions {
public:
    std::wstring subject;
    std::wstring from;
    std::wstring replyTo;
    std::wstring server = L"localhost";
    int port = 25;
    std::wstring user;
    std::wstring password;
    bool useSsl = false;
    bool isHtml = false;
    bool verbose = false;
    bool dryRun = false;
    int timeoutMs = 100000;
    std::vector<std::wstring> to;
    std::vector<std::wstring> cc;
    std::vector<std::wstring> bcc;
    std::vector<std::wstring> attachments;
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    std::wstring body;
    std::wstring bodyFile;
    bool showHelp = false;
    bool showVersion = false;

    static std::wstring Trim(const std::wstring& s) {
        size_t b = 0;
        while (b < s.size() && iswspace(s[b])) {
            ++b;
        }
        size_t e = s.size();
        while (e > b && iswspace(s[e - 1])) {
            --e;
        }
        return s.substr(b, e - b);
    }

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (arg == L"--help" || arg == L"-h") {
                showHelp = true;
                return true;
            }
            if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            }
            if (arg == L"--ssl") {
                useSsl = true;
                continue;
            }
            if (arg == L"--html") {
                isHtml = true;
                continue;
            }
            if (arg == L"-v") {
                verbose = true;
                continue;
            }
            if (arg == L"-n") {
                dryRun = true;
                continue;
            }

            if (arg == L"-s" || arg == L"-r" || arg == L"-R" || arg == L"-S" || arg == L"-u" || arg == L"-p" ||
                arg == L"-c" || arg == L"-b" || arg == L"-a" || arg == L"-m" || arg == L"-f" || arg == L"-H" || arg == L"-t") {
                if (i + 1 >= argc) {
                    std::wcerr << L"mailx: option requires an argument -- " << arg << L"\n";
                    return false;
                }

                std::wstring v = argv[++i] ? argv[i] : L"";
                if (arg == L"-s") {
                    subject = v;
                } else if (arg == L"-r") {
                    from = v;
                } else if (arg == L"-R") {
                    replyTo = v;
                } else if (arg == L"-S") {
                    size_t colon = v.find(L':');
                    if (colon == std::wstring::npos) {
                        server = v;
                    } else {
                        server = v.substr(0, colon);
                        port = _wtoi(v.substr(colon + 1).c_str());
                        if (port <= 0) {
                            std::wcerr << L"mailx: invalid SMTP port\n";
                            return false;
                        }
                    }
                } else if (arg == L"-u") {
                    user = v;
                } else if (arg == L"-p") {
                    password = v;
                } else if (arg == L"-c") {
                    cc.push_back(v);
                } else if (arg == L"-b") {
                    bcc.push_back(v);
                } else if (arg == L"-a") {
                    attachments.push_back(v);
                } else if (arg == L"-m") {
                    body = v;
                } else if (arg == L"-f") {
                    bodyFile = v;
                } else if (arg == L"-H") {
                    size_t sep = v.find(L':');
                    if (sep == std::wstring::npos) {
                        std::wcerr << L"mailx: invalid header format (expected NAME:VALUE)\n";
                        return false;
                    }
                    std::wstring name = Trim(v.substr(0, sep));
                    std::wstring value = Trim(v.substr(sep + 1));
                    if (name.empty()) {
                        std::wcerr << L"mailx: invalid header name\n";
                        return false;
                    }
                    headers.push_back({ name, value });
                } else if (arg == L"-t") {
                    timeoutMs = _wtoi(v.c_str());
                    if (timeoutMs <= 0) {
                        std::wcerr << L"mailx: timeout must be > 0\n";
                        return false;
                    }
                }
                continue;
            }

            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"mailx: unrecognized option '" << arg << L"'\n";
                return false;
            }

            to.push_back(arg);
        }

        return true;
    }

    void PrintHelp() const {
        std::wcout << LR"(mailx(1)                CrossShell for UNIX Reference Manual                 mailx(1)

    NAME
        mailx - send and receive Internet mail via SMTP/Windows Mail APIs

    SYNOPSIS
        mailx [OPTIONS] [RECIPIENT...]

    DESCRIPTION
        mailx is an interactive and command-line email utility supporting SMTP
        relay, attachments, and message composition.

    OPTIONS
        -s, --subject SUBJECT
            Specify subject on command line.

        -a, --attach FILE
            Attach FILE to the message.

        -r, --from ADDRESS
            Specify sender address.

        -c, --cc ADDRESS
            Specify Carbon Copy recipient.

        -b, --bcc ADDRESS
            Specify Blind Carbon Copy recipient.

        --json, --csv, --table
            Format dispatch results as JSON, CSV, or table.

        --pipe COMMAND
            Send dispatch diagnostics through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        mailx -s "Build Report" -a dist.zip dev@example.com < notes.txt
            Send email with subject and attachment.

    CrossShell for UNIX                                                  mailx(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"mailx 2.0.0\n";
    }
};

// ============================================================================
// 2. PAYLOAD BUILDER & POWERSHELL SCRIPT GENERATOR
// ============================================================================

class MailxPayloadBuilder {
public:
    static std::wstring QuoteCmdArg(const std::wstring& arg) {
        if (arg.empty()) {
            return L"\"\"";
        }
        bool needs = false;
        for (wchar_t ch : arg) {
            if (iswspace(ch) || ch == L'"') {
                needs = true;
                break;
            }
        }
        if (!needs) {
            return arg;
        }

        std::wstring out = L"\"";
        size_t bs = 0;
        for (wchar_t ch : arg) {
            if (ch == L'\\') {
                ++bs;
            } else if (ch == L'"') {
                out.append(bs * 2 + 1, L'\\');
                out.push_back(L'"');
                bs = 0;
            } else {
                out.append(bs, L'\\');
                bs = 0;
                out.push_back(ch);
            }
        }
        out.append(bs * 2, L'\\');
        out.push_back(L'"');
        return out;
    }

    static std::wstring ReadStdinAll() {
        std::wstringstream ss;
        std::wstring line;
        bool first = true;
        while (std::getline(std::wcin, line)) {
            if (!first) {
                ss << L"\n";
            }
            ss << line;
            first = false;
        }
        return ss.str();
    }

    static bool ReadFileAll(const std::wstring& path, std::wstring& out) {
        std::wifstream in(path);
        if (!in) {
            return false;
        }
        std::wstringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return true;
    }

    static bool FileExists(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    static std::wstring JsonEscape(const std::wstring& v) {
        std::wstring out;
        out.reserve(v.size() + 8);
        for (wchar_t ch : v) {
            switch (ch) {
                case L'\\': out += L"\\\\"; break;
                case L'"': out += L"\\\""; break;
                case L'\b': out += L"\\b"; break;
                case L'\f': out += L"\\f"; break;
                case L'\n': out += L"\\n"; break;
                case L'\r': out += L"\\r"; break;
                case L'\t': out += L"\\t"; break;
                default:
                    if (ch < 0x20) {
                        wchar_t buf[7] = {};
                        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"\\u%04x", static_cast<unsigned>(ch));
                        out += buf;
                    } else {
                        out.push_back(ch);
                    }
                    break;
            }
        }
        return out;
    }

    static void AppendJsonArray(std::wstringstream& ss, const std::vector<std::wstring>& items) {
        ss << L"[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i != 0) {
                ss << L",";
            }
            ss << L"\"" << JsonEscape(items[i]) << L"\"";
        }
        ss << L"]";
    }

    static bool WriteJsonPayload(const MailOptions& opt, const std::wstring& path) {
        std::wofstream out(path, std::ios::trunc);
        if (!out) {
            return false;
        }

        std::wstringstream ss;
        ss << L"{";
        ss << L"\"subject\":\"" << JsonEscape(opt.subject) << L"\",";
        ss << L"\"from\":\"" << JsonEscape(opt.from) << L"\",";
        ss << L"\"replyTo\":\"" << JsonEscape(opt.replyTo) << L"\",";
        ss << L"\"server\":\"" << JsonEscape(opt.server) << L"\",";
        ss << L"\"port\":" << opt.port << L",";
        ss << L"\"user\":\"" << JsonEscape(opt.user) << L"\",";
        ss << L"\"password\":\"" << JsonEscape(opt.password) << L"\",";
        ss << L"\"useSsl\":" << (opt.useSsl ? L"true" : L"false") << L",";
        ss << L"\"isHtml\":" << (opt.isHtml ? L"true" : L"false") << L",";
        ss << L"\"verbose\":" << (opt.verbose ? L"true" : L"false") << L",";
        ss << L"\"dryRun\":" << (opt.dryRun ? L"true" : L"false") << L",";
        ss << L"\"timeoutMs\":" << opt.timeoutMs << L",";
        ss << L"\"body\":\"" << JsonEscape(opt.body) << L"\",";

        ss << L"\"to\":";
        AppendJsonArray(ss, opt.to);
        ss << L",";
        ss << L"\"cc\":";
        AppendJsonArray(ss, opt.cc);
        ss << L",";
        ss << L"\"bcc\":";
        AppendJsonArray(ss, opt.bcc);
        ss << L",";
        ss << L"\"attachments\":";
        AppendJsonArray(ss, opt.attachments);
        ss << L",";

        ss << L"\"headers\": [";
        for (size_t i = 0; i < opt.headers.size(); ++i) {
            if (i != 0) {
                ss << L",";
            }
            ss << L"{\"name\":\"" << JsonEscape(opt.headers[i].first)
               << L"\",\"value\":\"" << JsonEscape(opt.headers[i].second) << L"\"}";
        }
        ss << L"]";

        ss << L"}";
        out << ss.str();
        return true;
    }

    static bool WritePsScript(const std::wstring& path) {
        std::wofstream out(path, std::ios::trunc);
        if (!out) {
            return false;
        }

        out << LR"PS(
param(
  [Parameter(Mandatory=$true)][string]$PayloadPath
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $PayloadPath)) {
  Write-Error "mailx: payload file not found"
  exit 126
}

$payload = Get-Content -LiteralPath $PayloadPath -Raw | ConvertFrom-Json

if (-not $payload.from -or [string]::IsNullOrWhiteSpace($payload.from)) {
  Write-Error "mailx: sender (-r FROM) is required"
  exit 1
}
if (-not $payload.to -or $payload.to.Count -eq 0) {
  Write-Error "mailx: at least one recipient is required"
  exit 1
}

$msg = New-Object System.Net.Mail.MailMessage
$msg.From = [System.Net.Mail.MailAddress]::new($payload.from)

foreach ($r in $payload.to) { if ($r) { [void]$msg.To.Add($r) } }
foreach ($r in $payload.cc) { if ($r) { [void]$msg.CC.Add($r) } }
foreach ($r in $payload.bcc) { if ($r) { [void]$msg.Bcc.Add($r) } }

if ($payload.replyTo -and -not [string]::IsNullOrWhiteSpace($payload.replyTo)) {
  [void]$msg.ReplyToList.Add($payload.replyTo)
}

$msg.Subject = if ($payload.subject) { [string]$payload.subject } else { '' }
$msg.Body = if ($payload.body) { [string]$payload.body } else { '' }
$msg.IsBodyHtml = [bool]$payload.isHtml
$msg.BodyEncoding = [System.Text.Encoding]::UTF8
$msg.SubjectEncoding = [System.Text.Encoding]::UTF8

foreach ($h in $payload.headers) {
  if ($h.name -and $h.value) {
    $msg.Headers[$h.name] = [string]$h.value
  }
}

foreach ($a in $payload.attachments) {
  if (-not (Test-Path -LiteralPath $a)) {
    Write-Error "mailx: attachment not found: $a"
    exit 1
  }
  [void]$msg.Attachments.Add([System.Net.Mail.Attachment]::new($a))
}

$smtp = [System.Net.Mail.SmtpClient]::new([string]$payload.server, [int]$payload.port)
$smtp.EnableSsl = [bool]$payload.useSsl
$smtp.DeliveryMethod = [System.Net.Mail.SmtpDeliveryMethod]::Network
$smtp.Timeout = [int]$payload.timeoutMs

if ($payload.user -and -not [string]::IsNullOrWhiteSpace($payload.user)) {
  $smtp.Credentials = [System.Net.NetworkCredential]::new([string]$payload.user, [string]$payload.password)
}

if ([bool]$payload.verbose) {
  $toList = [string]::Join(',', $payload.to)
  Write-Error "mailx: smtp=$($payload.server):$($payload.port) ssl=$($payload.useSsl) to=$toList"
}

if ([bool]$payload.dryRun) {
  Write-Output "mailx dry-run: message composed successfully"
  exit 0
}

$smtp.Send($msg)
Write-Output "mailx: message sent"
exit 0
)PS";

        return true;
    }

    static std::wstring GetTempFilePath(const wchar_t* prefix, const wchar_t* ext) {
        wchar_t tempPath[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, tempPath) == 0) {
            return L"";
        }

        wchar_t tempFile[MAX_PATH] = {};
        if (GetTempFileNameW(tempPath, prefix, 0, tempFile) == 0) {
            return L"";
        }

        std::wstring result = tempFile;
        if (ext && *ext) {
            DeleteFileW(result.c_str());
            result += ext;
        }
        return result;
    }
};

// ============================================================================
// 3. SMTP DISPATCHER
// ============================================================================

class SmtpDispatcher {
public:
    static int RunPowerShellMail(const std::wstring& scriptPath, const std::wstring& payloadPath) {
        std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File "
            + MailxPayloadBuilder::QuoteCmdArg(scriptPath) + L" -PayloadPath " + MailxPayloadBuilder::QuoteCmdArg(payloadPath);

        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            std::wcerr << L"mailx: failed to launch powershell SMTP sender\n";
            return 126;
        }

        ScopedProcessHandle hThread(pi.hThread);
        ScopedProcessHandle hProcess(pi.hProcess);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess.Get(), &exitCode);
        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class MailxApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        MailOptions opt;
        if (!opt.Parse(argc, argv)) {
            opt.PrintHelp();
            return 1;
        }

        if (opt.showHelp) {
            opt.PrintHelp();
            return 0;
        }

        if (opt.showVersion) {
            opt.PrintVersion();
            return 0;
        }

        if (opt.to.empty()) {
            std::wcerr << L"mailx: missing recipient\n";
            return 1;
        }
        if (opt.from.empty()) {
            std::wcerr << L"mailx: missing sender address (-r FROM)\n";
            return 1;
        }

        for (const auto& a : opt.attachments) {
            if (!MailxPayloadBuilder::FileExists(a)) {
                std::wcerr << L"mailx: attachment not found: " << a << L"\n";
                return 1;
            }
        }

        if (!opt.bodyFile.empty()) {
            if (!MailxPayloadBuilder::ReadFileAll(opt.bodyFile, opt.body)) {
                std::wcerr << L"mailx: failed to read body file: " << opt.bodyFile << L"\n";
                return 1;
            }
        } else if (opt.body.empty()) {
            opt.body = MailxPayloadBuilder::ReadStdinAll();
        }

        std::wstring payloadPath = MailxPayloadBuilder::GetTempFilePath(L"mlx", L".json");
        std::wstring scriptPath = MailxPayloadBuilder::GetTempFilePath(L"mlx", L".ps1");
        if (payloadPath.empty() || scriptPath.empty()) {
            std::wcerr << L"mailx: failed to create temporary files\n";
            return 125;
        }

        int rc = 125;
        if (!MailxPayloadBuilder::WriteJsonPayload(opt, payloadPath)) {
            std::wcerr << L"mailx: failed to write payload\n";
        } else if (!MailxPayloadBuilder::WritePsScript(scriptPath)) {
            std::wcerr << L"mailx: failed to write sender script\n";
        } else {
            rc = SmtpDispatcher::RunPowerShellMail(scriptPath, payloadPath);
        }

        DeleteFileW(payloadPath.c_str());
        DeleteFileW(scriptPath.c_str());
        return rc;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    MailxApplication app;
    return app.Run(argc, argv);
}
