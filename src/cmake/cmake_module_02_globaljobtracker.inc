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
