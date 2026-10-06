#include "engine.hpp"

static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
    if (size_needed <= 0) {
        size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        if (size_needed <= 0) return L"";
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
        return wstrTo;
    }
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

static std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        if (size_needed <= 0) return "";
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_ACP, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

static bool wildcard_match(const std::string& pattern, const std::string& text) {
    size_t p = 0, t = 0;
    size_t starP = std::string::npos, starT = std::string::npos;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            p++; t++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starP = p++;
            starT = t;
        } else if (starP != std::string::npos) {
            p = starP + 1;
            t = ++starT;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') p++;
    return p == pattern.size();
}

static void print_error_message(const std::string& message) {
    HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    bool hasConsole = hError != INVALID_HANDLE_VALUE && hError != NULL &&
        GetConsoleScreenBufferInfo(hError, &csbi);

    if (hasConsole) {
        SetConsoleTextAttribute(hError, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }

    std::cerr << message;
    std::cerr.flush();

    if (hasConsole) {
        SetConsoleTextAttribute(hError, csbi.wAttributes);
    }
}

// --- Dynamic Windows OS Release Query ---
static std::wstring get_registry_string(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return L"";
    }

    DWORD type = 0;
    DWORD size = 0;
    std::wstring value;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && size >= sizeof(wchar_t)) {
        value.resize(size / sizeof(wchar_t));
        if (RegQueryValueExW(key, value_name, nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value[0]), &size) == ERROR_SUCCESS) {
            while (!value.empty() && value.back() == L'\0') {
                value.pop_back();
            }
        } else {
            value.clear();
        }
    }

    RegCloseKey(key);
    return value;
}

std::wstring get_windows_release_text() {
    std::wstring product_name = get_registry_string(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"ProductName");
    std::wstring display_version = get_registry_string(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"DisplayVersion");

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionPtr rtl_get_version = (ntdll != nullptr)
        ? reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;

    RTL_OSVERSIONINFOEXW os = {};
    os.dwOSVersionInfoSize = sizeof(os);

    wchar_t version_text[96] = L"unknown";
    if (rtl_get_version != nullptr && rtl_get_version(reinterpret_cast<PRTL_OSVERSIONINFOW>(&os)) == 0) {
        _snwprintf_s(version_text, _countof(version_text), _TRUNCATE,
            L"%lu.%lu.%lu", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
    }

    std::wstring release = product_name.empty() ? L"Windows" : product_name;
    release += L" (";
    release += version_text;
    if (!display_version.empty()) {
        release += L", ";
        release += display_version;
    }
    release += L")";
    return release;
}


std::string TcshEngine::getHomeDirectory() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return wstring_to_string(path);
    }
    wchar_t userProfile[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return wstring_to_string(userProfile);
    return "C:\\";
}

TcshEngine::TcshEngine(bool loadRc) : builtins(tcsh_builtin_names()) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Enable VT100 / ANSI Escape Sequences for Console Colors
    enable_ansi_support();

    homeDir = getHomeDirectory();
    historyFile = homeDir + "\\.tcsh_history";
    logFile = homeDir + "\\.tcsh_log";
    rcFile = homeDir + "\\.tcshrc";

    setVariableList("prompt", { "% " });
    setVariableList("version", { "CrossShellTCSH 6.24.00" });
    setVariableList("status", { "0" });

    initFiles();
    loadHistory();
    if (loadRc) {
        loadRcFile();
    }
}


TcshEngine::~TcshEngine() {
    saveHistory();
    for (auto& job : jobList) {
        if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) CloseHandle(job.hProcess);
        if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) CloseHandle(job.hJob);
    }
}

void TcshEngine::initFiles() {
    std::ifstream checkRc(rcFile);
    if (!checkRc.good()) {
        std::string altRc = homeDir + "\\.cshrc";
        std::ifstream checkAlt(altRc);
        if (checkAlt.good()) {
            rcFile = altRc;
        } else {
            generateDefaultRcFile();
        }
    }

    std::ifstream checkHist(historyFile);
    if (!checkHist.good()) std::ofstream createHist(historyFile);
}

void TcshEngine::generateDefaultRcFile() {
    std::ofstream out(rcFile);
    if (out.is_open()) {
        out << "# tcsh Configuration File Auto-Generated\n";
        out << "# Default prompt: classic tcsh style ('% ' for user, '# ' for admin).\n";
        out << "# Add %~ or %n@%m if you prefer path/host context in your prompt.\n";
        out << "set prompt = \"% \"\n";
        out << "set history = 1000\n";
        out << "complete cd 'p/1/d/'\n";
        out.close();
        std::cout << "[tcsh] Auto-generated config file in: " << rcFile << "\n";
    }
}

void TcshEngine::loadHistory() {
    std::ifstream in(historyFile);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) history.push_back(line);
        if (history.size() >= kMaxHistoryEntries) break;
    }
    historyIndex = history.size();
}

void TcshEngine::saveHistory() {
    std::ofstream out(historyFile);
    size_t start = history.size() > kMaxHistoryEntries ? history.size() - kMaxHistoryEntries : 0;
    for (size_t i = start; i < history.size(); ++i) out << history[i] << "\n";
}

void TcshEngine::loadRcFile() {
    std::ifstream in(rcFile);
    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.pop_back();
        }

        if (trimmed == "set prompt = >") {
            line = "set prompt = \"% \"";
        }

        if (!line.empty() && line[0] != '#') executeCommandLine(line);
    }
}

std::string TcshEngine::resolveExecutable(const std::string& inputCmd, bool& foundOnDisk) {
    foundOnDisk = false;
    std::wstring wCmd = string_to_wstring(inputCmd);
    DWORD attr = GetFileAttributesW(wCmd.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        foundOnDisk = true;
        return inputCmd;
    }

    if (inputCmd.find('\\') != std::string::npos || inputCmd.find('/') != std::string::npos) {
        const std::vector<std::string> exts = { ".exe", ".cmd", ".bat", ".com" };
        for (const auto& ext : exts) {
            std::string testPath = inputCmd + ext;
            std::wstring wTestPath = string_to_wstring(testPath);
            attr = GetFileAttributesW(wTestPath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                foundOnDisk = true;
                return testPath;
            }
        }
    }

    wchar_t szPath[MAX_PATH];
    LPWSTR lpFilePart;
    if (SearchPathW(NULL, wCmd.c_str(), L".exe", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".cmd", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".bat", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".com", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }

    foundOnDisk = false;
    return inputCmd;
}


void TcshEngine::run() {
    // Keep startup close to real tcsh: minimal shell/version line before first prompt.
    std::cout << "CrossShellTCSH 6.24.00\n\n";

    while (running) {
        std::string line = readLineWithEditing();
        if (!line.empty()) {
            history.push_back(line);
            executeCommandLine(line);
            scriptDirective = ScriptDirective::None;
        }
    }
}

