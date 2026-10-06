#include "process_controller.hpp"
#include "process_info.hpp"
#include "scoped_handle.hpp"

[[nodiscard]] std::wstring ProcessController::getProcessOwner(HANDLE hProcess) {
        ScopedHandle hToken;
        HANDLE rawToken = nullptr;
        if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &rawToken)) {
            return L"<Unknown>";
        }
        hToken.reset(rawToken);

        DWORD len = 0;
        ::GetTokenInformation(hToken.get(), TokenUser, nullptr, 0, &len);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return L"<Unknown>";
        }

        std::vector<BYTE> buffer(len);
        if (!::GetTokenInformation(hToken.get(), TokenUser, buffer.data(), len, &len)) {
            return L"<Unknown>";
        }

        auto* tokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        WCHAR name[256];
        DWORD nameLen = 256;
        WCHAR domain[256];
        DWORD domainLen = 256;
        SID_NAME_USE use;

        if (::LookupAccountSidW(nullptr, tokenUser->User.Sid, name, &nameLen, domain, &domainLen, &use)) {
            return std::wstring(domain) + L"\\" + std::wstring(name);
        }
        return L"<Unknown>";
    }

[[nodiscard]] std::vector<ProcessInfo> ProcessController::snapshotProcesses() {
        std::vector<ProcessInfo> list;
        ScopedHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snapshot.isValid()) {
            return list;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (::Process32FirstW(snapshot.get(), &entry)) {
            do {
                if (entry.th32ProcessID == 0) continue;

                ProcessInfo info;
                info.pid = entry.th32ProcessID;
                info.name = entry.szExeFile;

                ScopedHandle hProc(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
                if (hProc.isValid()) {
                    info.currentPriorityClass = ::GetPriorityClass(hProc.get());
                    info.owner = getProcessOwner(hProc.get());
                } else {
                    info.currentPriorityClass = 0;
                    info.owner = L"<Access Denied>";
                }
                list.push_back(std::move(info));
            } while (::Process32NextW(snapshot.get(), &entry));
        }
        return list;
    }

bool ProcessController::applyPriority(DWORD pid, DWORD targetPriority, std::string& errorMsg) {
        ScopedHandle hProc(::OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (!hProc.isValid()) {
            DWORD err = ::GetLastError();
            errorMsg = "OpenProcess failed (Error code: " + std::to_string(err) + ")";
            return false;
        }

        if (!::SetPriorityClass(hProc.get(), targetPriority)) {
            DWORD err = ::GetLastError();
            errorMsg = "SetPriorityClass failed (Error code: " + std::to_string(err) + ")";
            return false;
        }

        return true;
    }
