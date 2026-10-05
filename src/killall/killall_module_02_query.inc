// ============================================================================
// Process Metadata & Query Engine
// ============================================================================
struct ProcessMeta {
    DWORD pid;
    DWORD parentPid;
    std::string name;
    std::string owner;
    FILETIME creationTime{};
    std::chrono::seconds age{0};
};

class ProcessQueryEngine {
public:
    static std::string getProcessOwner(HANDLE hProcess) {
        HANDLE hToken = nullptr;
        if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
            return "";
        }
        UniqueHandle tokenHandle(hToken);

        DWORD len = 0;
        ::GetTokenInformation(tokenHandle.get(), TokenUser, nullptr, 0, &len);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) return "";

        std::vector<BYTE> buffer(len);
        if (!::GetTokenInformation(tokenHandle.get(), TokenUser, buffer.data(), len, &len)) {
            return "";
        }

        auto* pTokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        WCHAR name[256] = {0};
        WCHAR domain[256] = {0};
        DWORD nameLen = 256, domainLen = 256;
        SID_NAME_USE snu;

        if (::LookupAccountSidW(nullptr, pTokenUser->User.Sid, name, &nameLen, domain, &domainLen, &snu)) {
            std::string utf8Name;
            int req = ::WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            if (req > 0) {
                utf8Name.resize(req - 1);
                ::WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8Name.data(), req, nullptr, nullptr);
            }
            return utf8Name;
        }
        return "";
    }

    static std::vector<ProcessMeta> collectProcesses() {
        std::vector<ProcessMeta> list;
        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return list;
        UniqueHandle snapHandle(hSnap);

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (::Process32FirstW(snapHandle.get(), &pe)) {
            do {
                if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) continue; // Skip System Idle / System

                std::string exeName;
                int len = ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    exeName.resize(len - 1);
                    ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exeName.data(), len, nullptr, nullptr);
                }

                ProcessMeta meta;
                meta.pid = pe.th32ProcessID;
                meta.parentPid = pe.th32ParentProcessID;
                meta.name = exeName;

                // Inspect times & owner if permissions allow
                HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, meta.pid);
                if (hProc) {
                    UniqueHandle procHandle(hProc);
                    FILETIME exitTime, kernelTime, userTime;
                    if (::GetProcessTimes(procHandle.get(), &meta.creationTime, &exitTime, &kernelTime, &userTime)) {
                        meta.age = TimeHelper::getProcessAge(meta.creationTime);
                    }
                    meta.owner = getProcessOwner(procHandle.get());
                }

                list.push_back(std::move(meta));
            } while (::Process32NextW(snapHandle.get(), &pe));
        }
        return list;
    }
};

