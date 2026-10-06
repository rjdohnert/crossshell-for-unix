#include "acl_engine.hpp"
#include "principal_resolver.hpp"

bool AclEngine::ApplyEntry(const std::wstring& path, const FaclEntry& entry) {
    std::vector<BYTE> sidBuf;
    std::vector<wchar_t> domainBuf;
    PSID sid = nullptr;
    bool isAllocatedSid = false;

    if (entry.kind == L"o") {
        sid = PrincipalResolver::AllocateEveryoneSid();
        if (!sid) {
            std::wcerr << L"setfacl: failed to resolve everyone SID for " << path << L"\n";
            return false;
        }
        isAllocatedSid = true;
    } else {
        sid = PrincipalResolver::NameToSid(entry.name, sidBuf, domainBuf);
        if (!sid) {
            std::wcerr << L"setfacl: could not resolve principal '" << entry.name << L"'\n";
            return false;
        }
    }

    EXPLICIT_ACCESS_W ea{};
    ea.grfAccessPermissions = PrincipalResolver::RightsToMask(entry.rights);
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = (entry.kind == L"g") ? TRUSTEE_IS_GROUP : TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

    PACL newDacl = nullptr;
    DWORD res = SetEntriesInAclW(1, &ea, nullptr, &newDacl);
    if (res != ERROR_SUCCESS) {
        if (isAllocatedSid) FreeSid(sid);
        std::wcerr << L"setfacl: failed to build ACL for '" << path << L"'\n";
        return false;
    }

    res = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(path.c_str()),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        newDacl,
        nullptr
    );

    if (newDacl) LocalFree(newDacl);
    if (isAllocatedSid) FreeSid(sid);

    if (res != ERROR_SUCCESS) {
        std::wcerr << L"setfacl: failed to apply ACL to '" << path << L"' (" << res << L")\n";
        return false;
    }

    std::wcout << L"setfacl: updated ACL for " << path << L"\n";
    return true;
}

bool AclEngine::RemoveDacl(const std::wstring& path) {
    DWORD res = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(path.c_str()),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (res != ERROR_SUCCESS) {
        std::wcerr << L"setfacl: failed to clear ACL for '" << path << L"' (" << res << L")\n";
        return false;
    }

    std::wcout << L"setfacl: cleared ACL for " << path << L"\n";
    return true;
}

bool AclEngine::ProcessDirectoryRecursive(const std::wstring& dirPath, bool removeDacl, const FaclEntry& entry) {
    std::wstring pattern = dirPath;
    if (!pattern.empty() && pattern.back() != L'\\') {
        pattern.push_back(L'\\');
    }
    pattern.push_back(L'*');

    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return true;
    }

    bool ok = true;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        std::wstring child = dirPath;
        if (!child.empty() && child.back() != L'\\') {
            child.push_back(L'\\');
        }
        child += findData.cFileName;

        if (removeDacl) {
            if (!RemoveDacl(child)) ok = false;
        } else {
            if (!ApplyEntry(child, entry)) ok = false;
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!ProcessDirectoryRecursive(child, removeDacl, entry)) {
                ok = false;
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return ok;
}
