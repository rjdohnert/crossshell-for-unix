class SecurityEngine {
public:
    static bool GrantAppContainerAccess(const fs::path& targetPath, DWORD permissions) {
        PSID pSidPackages = nullptr;
        if (!ConvertStringSidToSidW(L"S-1-15-2-1", &pSidPackages)) {
            return false;
        }

        EXPLICIT_ACCESSW ea = { 0 };
        ea.grfAccessPermissions = permissions;
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea.Trustee.ptstrName = (LPWSTR)pSidPackages;

        PACL pOldAcl = nullptr;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        GetNamedSecurityInfoW(targetPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOldAcl, nullptr, &pSD);

        PACL pNewAcl = nullptr;
        DWORD res = SetEntriesInAclW(1, &ea, pOldAcl, &pNewAcl);
        if (res == ERROR_SUCCESS) {
            SetNamedSecurityInfoW((LPWSTR)targetPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewAcl, nullptr);
            LocalFree(pNewAcl);
        }

        if (pSD) LocalFree(pSD);
        LocalFree(pSidPackages);
        return (res == ERROR_SUCCESS);
    }

    static void EnsureBinaryReadable(const std::wstring& command) {
        std::wstring rawExe;
        if (command.empty()) return;

        if (command[0] == L'"') {
            size_t endQuote = command.find(L'"', 1);
            if (endQuote != std::wstring::npos) {
                rawExe = command.substr(1, endQuote - 1);
            }
        } else {
            size_t pos = command.find(L' ');
            while (pos != std::wstring::npos) {
                std::wstring prefix = command.substr(0, pos);
                if (fs::exists(prefix)) {
                    rawExe = prefix;
                    break;
                }
                wchar_t resolved[MAX_PATH] = { 0 };
                if (SearchPathW(nullptr, prefix.c_str(), L".exe", MAX_PATH, resolved, nullptr) > 0) {
                    rawExe = prefix;
                    break;
                }
                pos = command.find(L' ', pos + 1);
            }
            if (rawExe.empty()) {
                pos = command.find(L' ');
                rawExe = (pos != std::wstring::npos) ? command.substr(0, pos) : command;
            }
        }

        if (rawExe.empty()) return;

        // Resolve absolute path or search PATH environment
        wchar_t resolvedPath[MAX_PATH] = { 0 };
        if (SearchPathW(nullptr, rawExe.c_str(), L".exe", MAX_PATH, resolvedPath, nullptr) > 0) {
            GrantAppContainerAccess(resolvedPath, GENERIC_READ | GENERIC_EXECUTE);
        } else if (fs::exists(rawExe)) {
            GrantAppContainerAccess(rawExe, GENERIC_READ | GENERIC_EXECUTE);
        }
    }

    static std::vector<wchar_t> CreateSandboxedEnvironmentBlock(const fs::path& workspacePath) {
        LPWCH pEnvStrings = GetEnvironmentStringsW();
        if (!pEnvStrings) return {};

        std::vector<std::wstring> envVars;
        LPWCH current = pEnvStrings;
        while (*current) {
            std::wstring var(current);
            // Filter out existing temp and appdata variables
            if (var.rfind(L"TEMP=", 0) != 0 &&
                var.rfind(L"TMP=", 0) != 0 &&
                var.rfind(L"LOCALAPPDATA=", 0) != 0) {
                envVars.push_back(var);
            }
            current += wcslen(current) + 1;
        }
        FreeEnvironmentStringsW(pEnvStrings);

        // Redirect TEMP, TMP, and LOCALAPPDATA directly to the sandboxed workspace
        envVars.push_back(L"TEMP=" + workspacePath.wstring());
        envVars.push_back(L"TMP=" + workspacePath.wstring());
        envVars.push_back(L"LOCALAPPDATA=" + workspacePath.wstring());

        // Serialize environment block (double null terminated)
        std::vector<wchar_t> envBlock;
        for (const auto& var : envVars) {
            envBlock.insert(envBlock.end(), var.begin(), var.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');

        return envBlock;
    }

    static HANDLE CreateJobSandbox(IsolationLevel level) {
        if (level == IsolationLevel::Normal) {
            return nullptr;
        }

        HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
        if (!hJob) return nullptr;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | 
                                                JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

        if (level == IsolationLevel::TotalLockdown || level == IsolationLevel::EphemeralNet) {
            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            jeli.BasicLimitInformation.ActiveProcessLimit = (level == IsolationLevel::TotalLockdown) ? 4 : 16;

            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
            jeli.JobMemoryLimit = (level == IsolationLevel::TotalLockdown)
                ? static_cast<SIZE_T>(512ULL * 1024ULL * 1024ULL)
                : static_cast<SIZE_T>(2ULL * 1024ULL * 1024ULL * 1024ULL);
        }

        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));

        if (level == IsolationLevel::TotalLockdown || level == IsolationLevel::EphemeralNet) {
            JOBOBJECT_BASIC_UI_RESTRICTIONS ui = { 0 };
            ui.UIRestrictionsClass = JOB_OBJECT_UILIMIT_HANDLES |
                                     JOB_OBJECT_UILIMIT_READCLIPBOARD |
                                     JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
                                     JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                                     JOB_OBJECT_UILIMIT_DESKTOP |
                                     JOB_OBJECT_UILIMIT_EXITWINDOWS;
            SetInformationJobObject(hJob, JobObjectBasicUIRestrictions, &ui, sizeof(ui));
        }

        return hJob;
    }
};

// --- Execution Dispatcher ---
