#include "engine.hpp"
#include <fstream>
#include <iostream>
#include <cwctype>

std::wstring MailxPayloadBuilder::QuoteCmdArg(const std::wstring& arg) {
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

std::wstring MailxPayloadBuilder::ReadStdinAll() {
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

bool MailxPayloadBuilder::ReadFileAll(const std::wstring& path, std::wstring& out) {
    std::wifstream in(path);
    if (!in) {
        return false;
    }
    std::wstringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool MailxPayloadBuilder::FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring MailxPayloadBuilder::JsonEscape(const std::wstring& v) {
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

void MailxPayloadBuilder::AppendJsonArray(std::wstringstream& ss, const std::vector<std::wstring>& items) {
    ss << L"[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            ss << L",";
        }
        ss << L"\"" << JsonEscape(items[i]) << L"\"";
    }
    ss << L"]";
}

bool MailxPayloadBuilder::WriteJsonPayload(const MailOptions& opt, const std::wstring& path) {
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

bool MailxPayloadBuilder::WritePsScript(const std::wstring& path) {
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

std::wstring MailxPayloadBuilder::GetTempFilePath(const wchar_t* prefix, const wchar_t* ext) {
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

int SmtpDispatcher::RunPowerShellMail(const std::wstring& scriptPath, const std::wstring& payloadPath) {
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
