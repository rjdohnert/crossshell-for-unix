std::wstring escape_win32_arg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    bool needs_quotes = false;
    for (wchar_t c : arg) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return arg;

    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            backslashes++;
        } else if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring to_lower_w(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

std::wstring resolve_executable(const std::wstring& cmd) {
    wchar_t pathext_buf[32767];
    DWORD pathext_len = GetEnvironmentVariableW(L"PATHEXT", pathext_buf, 32767);
    std::vector<std::wstring> extensions = { L".exe", L".cmd", L".bat", L".com" };
    if (pathext_len > 0) {
        std::wstring pathext_wstr(pathext_buf, pathext_len);
        std::transform(pathext_wstr.begin(), pathext_wstr.end(), pathext_wstr.begin(), ::towlower);
        std::wstringstream ss(pathext_wstr);
        std::wstring ext;
        extensions.clear();
        while (std::getline(ss, ext, L';')) {
            if (!ext.empty()) extensions.push_back(ext);
        }
    }

    auto try_extensions = [&](const std::wstring& base) -> std::wstring {
        DWORD attr = GetFileAttributesW(base.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) return base;
        for (const auto& ext : extensions) {
            std::wstring candidate = base + ext;
            DWORD cattr = GetFileAttributesW(candidate.c_str());
            if (cattr != INVALID_FILE_ATTRIBUTES && !(cattr & FILE_ATTRIBUTE_DIRECTORY)) {
                return candidate;
            }
        }
        return L"";
    };

    if (cmd.find(L'\\') != std::wstring::npos || cmd.find(L'/') != std::wstring::npos) {
        std::wstring res = try_extensions(cmd);
        if (!res.empty()) return res;
    }

    wchar_t buf[MAX_PATH];
    for (const auto& ext : extensions) {
        DWORD len = SearchPathW(NULL, cmd.c_str(), ext.c_str(), MAX_PATH, buf, NULL);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
    }

    DWORD len = SearchPathW(NULL, cmd.c_str(), NULL, MAX_PATH, buf, NULL);
    if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);

    return L"";
}

bool is_batch_file(const std::wstring& path) {
    std::wstring ext = to_lower_w(path);
    return (ext.length() >= 4 && ext.substr(ext.length() - 4) == L".bat") ||
           (ext.length() >= 4 && ext.substr(ext.length() - 4) == L".cmd");
}

// ============================================================================
// DURATION & SIGNAL PARSING
// ============================================================================

bool parse_duration(const std::wstring& str, double& seconds_out) {
    if (str.empty()) return false;
    std::wstring num_part = str;
    wchar_t suffix = L's';

    wchar_t last_c = ::towlower(str.back());
    if (last_c == L's' || last_c == L'm' || last_c == L'h' || last_c == L'd') {
        suffix = last_c;
        num_part = str.substr(0, str.length() - 1);
    }

    try {
        size_t idx = 0;
        double val = std::stod(num_part, &idx);
        if (idx != num_part.length() || val < 0.0) return false;

        switch (suffix) {
            case L's': seconds_out = val; break;
            case L'm': seconds_out = val * 60.0; break;
            case L'h': seconds_out = val * 3600.0; break;
            case L'd': seconds_out = val * 86400.0; break;
            default: return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_signal(const std::wstring& str, DWORD& signal_out) {
    std::wstring s = to_lower_w(str);
    if (s.rfind(L"sig", 0) == 0) s = s.substr(3);

    static const std::map<std::wstring, DWORD> sig_map = {
        { L"hup", SIG_HUP },   { L"1", SIG_HUP },
        { L"int", SIG_INT },   { L"2", SIG_INT },
        { L"quit", SIG_QUIT }, { L"3", SIG_QUIT },
        { L"kill", SIG_KILL }, { L"9", SIG_KILL },
        { L"term", SIG_TERM }, { L"15", SIG_TERM }
    };

    auto it = sig_map.find(s);
    if (it != sig_map.end()) {
        signal_out = it->second;
        return true;
    }

    try {
        size_t idx = 0;
        DWORD val = std::stoul(str, &idx);
        if (idx == str.length()) {
            signal_out = val;
            return true;
        }
    } catch (...) {}

    return false;
}

std::wstring get_signal_name(DWORD sig) {
    switch (sig) {
        case SIG_HUP:  return L"SIGHUP";
        case SIG_INT:  return L"SIGINT";
        case SIG_QUIT: return L"SIGQUIT";
        case SIG_KILL: return L"SIGKILL";
        case SIG_TERM: return L"SIGTERM";
        default:       return L"SIG" + std::to_wstring(sig);
    }
}

