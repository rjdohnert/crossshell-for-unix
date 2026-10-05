// ============================================================================
// Process Enumeration & Resolution Helper
// ============================================================================
struct TargetProcess {
    DWORD pid;
    std::string name;
    DWORD parentPid;
};

class ProcessEnumerator {
public:
    static std::vector<TargetProcess> getSnapshot() {
        std::vector<TargetProcess> list;
        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return list;
        UniqueHandle snapHandle(hSnap);

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (::Process32FirstW(snapHandle.get(), &pe)) {
            do {
                std::string exeName;
                int len = ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    exeName.resize(len - 1);
                    ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exeName.data(), len, nullptr, nullptr);
                }
                list.push_back({ pe.th32ProcessID, exeName, pe.th32ParentProcessID });
            } while (::Process32NextW(snapHandle.get(), &pe));
        }
        return list;
    }

    static std::vector<DWORD> resolveTarget(const std::string& targetStr, bool includeTree) {
        std::set<DWORD> resolvedPids;
        auto snapshot = getSnapshot();

        bool isNumeric = !targetStr.empty() && std::all_of(targetStr.begin(), targetStr.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c));
        });

        if (isNumeric) {
            DWORD pid = static_cast<DWORD>(std::stoul(targetStr));
            resolvedPids.insert(pid);
        } else {
            std::string query = targetStr;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            for (const auto& proc : snapshot) {
                std::string procNameLower = proc.name;
                std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), ::tolower);
                if (procNameLower == query || procNameLower == query + ".exe") {
                    resolvedPids.insert(proc.pid);
                }
            }
        }

        if (includeTree) {
            std::set<DWORD> treePids = resolvedPids;
            for (DWORD rootPid : resolvedPids) {
                collectChildren(rootPid, snapshot, treePids);
            }
            resolvedPids = std::move(treePids);
        }

        return std::vector<DWORD>(resolvedPids.begin(), resolvedPids.end());
    }

private:
    static void collectChildren(DWORD parentPid, const std::vector<TargetProcess>& snapshot, std::set<DWORD>& outPids) {
        for (const auto& proc : snapshot) {
            if (proc.parentPid == parentPid && proc.pid != 0 && proc.pid != parentPid) {
                if (outPids.insert(proc.pid).second) {
                    collectChildren(proc.pid, snapshot, outPids);
                }
            }
        }
    }
};

