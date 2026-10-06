#include "reporter.hpp"

static volatile LONG g_cancelRequested = 0;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            InterlockedExchange(&g_cancelRequested, 1);
            return TRUE;
        default:
            return FALSE;
    }
}

bool NfsOutputFormatter::IsCancelled() {
    return InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0;
}

bool NfsOutputFormatter::IsDeadlineExceeded(ULONGLONG startTick, const GlobalOptions& options) {
    if (options.opTimeoutMs <= 0) return false;
    ULONGLONG elapsed = GetTickCount64() - startTick;
    return elapsed > static_cast<ULONGLONG>(options.opTimeoutMs);
}

DWORD NfsOutputFormatter::CheckOperationState(ULONGLONG startTick, const GlobalOptions& options) {
    if (IsCancelled()) return ERROR_CANCELLED;
    if (IsDeadlineExceeded(startTick, options)) return WAIT_TIMEOUT;
    return ERROR_SUCCESS;
}

void NfsOutputFormatter::LogInfo(const GlobalOptions& options, const wchar_t* fmt, ...) {
    if (options.quiet) return;
    va_list args;
    va_start(args, fmt);
    vwprintf(fmt, args);
    va_end(args);
}

void NfsOutputFormatter::LogVerbose(const GlobalOptions& options, const wchar_t* fmt, ...) {
    if (!options.verbose && !options.trace) return;
    va_list args;
    va_start(args, fmt);
    vwprintf(fmt, args);
    va_end(args);
}

void NfsOutputFormatter::LogTrace(const GlobalOptions& options, const wchar_t* fmt, ...) {
    if (!options.trace) return;
    va_list args;
    va_start(args, fmt);
    vwprintf(fmt, args);
    va_end(args);
}

std::wstring NfsOutputFormatter::ToLowerWide(std::wstring input) {
    std::transform(input.begin(), input.end(), input.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return input;
}

bool NfsOutputFormatter::ContainsIcase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    return ToLowerWide(haystack).find(ToLowerWide(needle)) != std::wstring::npos;
}

std::wstring NfsOutputFormatter::JsonEscape(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size() + 8);
    for (wchar_t c : input) {
        switch (c) {
            case L'\\': out += L"\\\\"; break;
            case L'\"': out += L"\\\""; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::wstring NfsOutputFormatter::RegistryTypeLabel(DWORD type) {
    switch (type) {
        case REG_SZ: return L"REG_SZ";
        case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
        case REG_MULTI_SZ: return L"REG_MULTI_SZ";
        case REG_DWORD: return L"REG_DWORD";
        case REG_QWORD: return L"REG_QWORD";
        case REG_BINARY: return L"REG_BINARY";
        case REG_NONE: return L"REG_NONE";
        default: return L"REG_UNKNOWN";
    }
}

std::wstring NfsOutputFormatter::RegistryStringFromData(const BYTE* data, DWORD size) {
    if (data == NULL || size == 0) return L"";
    const wchar_t* text = reinterpret_cast<const wchar_t*>(data);
    size_t charCount = size / sizeof(wchar_t);
    std::wstring value(text, text + charCount);
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

std::wstring NfsOutputFormatter::RegistryBinaryToHex(const BYTE* data, DWORD size) {
    static const wchar_t* kHex = L"0123456789ABCDEF";
    std::wstring out;
    out.reserve(static_cast<size_t>(size) * 2 + 2);
    out += L"0x";
    for (DWORD i = 0; i < size; ++i) {
        BYTE b = data[i];
        out.push_back(kHex[(b >> 4) & 0xF]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

void NfsOutputFormatter::AppendRegistryValueJson(std::wstringstream& out, DWORD type, const BYTE* data, DWORD size) {
    switch (type) {
        case REG_DWORD: {
            DWORD value = 0;
            if (data != NULL && size >= sizeof(DWORD)) {
                memcpy(&value, data, sizeof(DWORD));
            }
            out << value;
            return;
        }
        case REG_QWORD: {
            ULONGLONG value = 0;
            if (data != NULL && size >= sizeof(ULONGLONG)) {
                memcpy(&value, data, sizeof(ULONGLONG));
            }
            out << value;
            return;
        }
        case REG_MULTI_SZ: {
            std::wstring multi = RegistryStringFromData(data, size);
            out << L"[";
            bool first = true;
            size_t pos = 0;
            while (pos < multi.size()) {
                size_t next = multi.find(L'\0', pos);
                if (next == std::wstring::npos) next = multi.size();
                std::wstring part = multi.substr(pos, next - pos);
                if (part.empty()) break;
                if (!first) out << L",";
                first = false;
                out << L'"' << JsonEscape(part) << L'"';
                pos = next + 1;
            }
            out << L"]";
            return;
        }
        case REG_SZ:
        case REG_EXPAND_SZ:
        default: {
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                out << L'"' << JsonEscape(RegistryStringFromData(data, size)) << L'"';
            } else {
                out << L'"' << JsonEscape(RegistryBinaryToHex(data, size)) << L'"';
            }
            return;
        }
    }
}

DWORD NfsOutputFormatter::BuildRegistryValuesJson(HKEY hKeyRoot, const wchar_t* subKey, std::wstring& outJson, DWORD& valueCount) {
    outJson.clear();
    valueCount = 0;

    ScopedRegistryKey hKey;
    DWORD dwErr = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.Receive());
    if (dwErr != ERROR_SUCCESS) {
        outJson = L"[]";
        return dwErr;
    }

    DWORD maxValueNameLen = 0;
    DWORD maxValueDataLen = 0;
    dwErr = RegQueryInfoKeyW(hKey.Get(), NULL, NULL, NULL, NULL, NULL, NULL, &valueCount,
                             &maxValueNameLen, &maxValueDataLen, NULL, NULL);
    if (dwErr != ERROR_SUCCESS) {
        outJson = L"[]";
        return dwErr;
    }

    std::vector<wchar_t> nameBuffer(static_cast<size_t>(maxValueNameLen) + 2, L'\0');
    std::vector<BYTE> dataBuffer(std::max<DWORD>(maxValueDataLen, static_cast<DWORD>(sizeof(ULONGLONG))) + 2, 0);
    std::wstringstream payload;
    payload << L"[";
    bool first = true;

    for (DWORD index = 0; index < valueCount; ++index) {
        DWORD nameLen = static_cast<DWORD>(nameBuffer.size());
        DWORD type = 0;
        DWORD dataLen = static_cast<DWORD>(dataBuffer.size());
        dwErr = RegEnumValueW(hKey.Get(), index, nameBuffer.data(), &nameLen, NULL, &type, dataBuffer.data(), &dataLen);
        if (dwErr != ERROR_SUCCESS) {
            continue;
        }

        if (!first) payload << L",";
        first = false;
        payload << L"{\"name\":\"" << JsonEscape(std::wstring(nameBuffer.data(), nameLen))
                << L"\",\"type\":\"" << RegistryTypeLabel(type) << L"\",\"value\":";
        AppendRegistryValueJson(payload, type, dataBuffer.data(), dataLen);
        payload << L"}";
    }

    payload << L"]";
    outJson = payload.str();
    return ERROR_SUCCESS;
}

std::wstring NfsOutputFormatter::Win32ErrorMessage(DWORD code) {
    LPWSTR buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, NULL, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
    if (len == 0 || buffer == nullptr) {
        return L"Unknown error";
    }
    std::wstring msg(buffer, len);
    LocalFree(buffer);
    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) {
        msg.pop_back();
    }
    return msg;
}

void NfsOutputFormatter::PrintWin32Error(const wchar_t* context, DWORD code) {
    fwprintf(stderr, L"Error: %s: %s (Win32 Error %lu)\n", context, Win32ErrorMessage(code).c_str(), code);
}

int NfsOutputFormatter::NormalizeExitCode(DWORD code) {
    if (code == ERROR_SUCCESS) return 0;
    if (code == ERROR_INVALID_PARAMETER || code == ERROR_BAD_ARGUMENTS) return 1;
    if (code == ERROR_ACCESS_DENIED || code == ERROR_ELEVATION_REQUIRED || code == ERROR_CANCELLED) return 2;
    if (code == ERROR_NETWORK_UNREACHABLE || code == ERROR_HOST_UNREACHABLE || code == ERROR_CONNECTION_REFUSED || code == ERROR_DEV_NOT_EXIST) return 3;
    return 4;
}

std::wstring NfsOutputFormatter::AddressFamilyLabel(int addressFamily) {
    if (addressFamily == AF_INET6) return L"ipv6";
    if (addressFamily == AF_UNSPEC) return L"dual";
    return L"ipv4";
}

std::wstring NfsOutputFormatter::NfsProbeModeLabel(NfsProbeMode mode) {
    switch (mode) {
        case NfsProbeMode::V3: return L"v3";
        case NfsProbeMode::V4: return L"v4";
        case NfsProbeMode::Both:
        default: return L"both";
    }
}

void NfsOutputFormatter::PrintDelimitedCell(const std::wstring& value, OutputMode mode) {
    if (mode == OutputMode::Csv) {
        std::wstring escaped;
        escaped.reserve(value.size() + 4);
        for (wchar_t c : value) {
            if (c == L'"') escaped += L"\"\"";
            else escaped.push_back(c);
        }
        wprintf(L"\"%s\"", escaped.c_str());
    } else {
        std::wstring clean = value;
        std::replace(clean.begin(), clean.end(), L'\t', L' ');
        std::replace(clean.begin(), clean.end(), L'\n', L' ');
        wprintf(L"%s", clean.c_str());
    }
}

void NfsOutputFormatter::PrintDelimitedRow(const std::vector<std::wstring>& cells, OutputMode mode) {
    const wchar_t* sep = (mode == OutputMode::Csv) ? L"," : L"\t";
    for (size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) wprintf(L"%s", sep);
        PrintDelimitedCell(cells[i], mode);
    }
    wprintf(L"\n");
}

void NfsOutputFormatter::PrintJsonEnvelope(const std::wstring& command, DWORD code, const std::wstring& dataJson, const std::wstring& errorMessage) {
    std::wstring err = errorMessage.empty() ? Win32ErrorMessage(code) : errorMessage;
    std::wstring errJson = L"null";
    if (code != ERROR_SUCCESS) {
        errJson = L"\"" + JsonEscape(err) + L"\"";
    }
    wprintf(L"{\"ok\":%s,\"code\":%lu,\"command\":\"%s\",\"data\":%s,\"error\":%s}\n",
            (code == ERROR_SUCCESS) ? L"true" : L"false",
            code,
            JsonEscape(command).c_str(),
            dataJson.c_str(),
            errJson.c_str());
}
