std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
    return out;
}

std::wstring to_lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return s;
}

std::wstring resolve_log(const std::wstring& in) {
    std::wstring v = to_lower(in);
    if (v == L"system") return L"System";
    if (v == L"application" || v == L"app") return L"Application";
    if (v == L"security" || v == L"sec") return L"Security";
    return L"";
}

std::string severity_word(WORD t) {
    switch (t) {
    case EVENTLOG_ERROR_TYPE: return "ERROR";
    case EVENTLOG_WARNING_TYPE: return "WARN";
    case EVENTLOG_INFORMATION_TYPE: return "INFO";
    case EVENTLOG_AUDIT_SUCCESS: return "AUDIT_OK";
    case EVENTLOG_AUDIT_FAILURE: return "AUDIT_FAIL";
    default: return "OTHER";
    }
}

std::string format_time(DWORD ts) {
    std::time_t when = static_cast<std::time_t>(ts);
    std::tm localTm{};
    localtime_s(&localTm, &when);
    char buf[64]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTm);
    return std::string(buf);
}

std::string first_insertion_string(const EVENTLOGRECORD* rec) {
    if (rec->NumStrings == 0 || rec->StringOffset == 0) {
        return "";
    }
    const wchar_t* p = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(rec) + rec->StringOffset);
    if (*p == L'\0') return "";
    return to_utf8(p);
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) out += ' ';
            else out += static_cast<char>(ch);
        }
    }
    return out;
}

std::string csv_escape(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') out += "\"\"";
        else out += ch;
    }
    return out + "\"";
}
