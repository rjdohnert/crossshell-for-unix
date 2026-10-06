#include "delete_options.hpp"
#include "security_utils.hpp"
#include "string_utils.hpp"
#include "windows_user_manager.hpp"

bool WindowsUserManager::TerminateUserProcesses(const std::string& username, bool verbose) {
        auto optSid = SecurityUtils::GetUserSidString(username);
        if (!optSid.has_value()) return false;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID <= 4) continue; // Skip System/Idle

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    HANDLE hToken = NULL;
                    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
                        DWORD len = 0;
                        GetTokenInformation(hToken, TokenUser, NULL, 0, &len);
                        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                            std::vector<BYTE> buffer(len);
                            PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                            if (GetTokenInformation(hToken, TokenUser, pTokenUser, len, &len)) {
                                LPWSTR procSidStr = NULL;
                                if (ConvertSidToStringSidW(pTokenUser->User.Sid, &procSidStr)) {
                                    if (optSid.value() == procSidStr) {
                                        if (verbose) {
                                            std::wcout << L"[FORCE] Terminating process: " << pe.szExeFile 
                                                       << L" (PID: " << pe.th32ProcessID << L")\n";
                                        }
                                        TerminateProcess(hProcess, 1);
                                    }
                                    LocalFree(procSidStr);
                                }
                            }
                        }
                        CloseHandle(hToken);
                    }
                    CloseHandle(hProcess);
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
        return true;
    }

bool WindowsUserManager::DeleteUserProfile(const std::string& username, bool verbose) {
        auto optSid = SecurityUtils::GetUserSidString(username);
        if (!optSid.has_value()) return false;

        if (verbose) {
            std::wcout << L"Removing User Profile with SID: " << optSid.value() << L"\n";
        }

        // Delete official user profile via Win32 UserEnv API
        BOOL status = DeleteProfileW(optSid.value().c_str(), NULL, NULL);
        return (status == TRUE);
    }

bool WindowsUserManager::DeleteHomeDirectory(const std::string& username, const std::string& rootDir, bool verbose) {
        fs::path userHome = rootDir.empty() 
            ? (fs::path("C:\\Users") / username) 
            : (fs::path(rootDir) / username);

        try {
            if (fs::exists(userHome)) {
                if (verbose) {
                    std::cout << "Removing home directory: " << userHome.string() << "\n";
                }
                fs::remove_all(userHome);
                return true;
            }
        } catch (const std::exception& e) {
            if (verbose) {
                std::cerr << "[WARNING] Failed removing home directory: " << e.what() << "\n";
            }
            return false;
        }
        return false;
    }

bool WindowsUserManager::DeleteAccount(const DeleteOptions& opt, std::string& errorMessage) {
        std::wstring wUsername = StringUtils::ToWide(opt.username);

        // Terminate active processes if force requested
        if (opt.force) {
            TerminateUserProcesses(opt.username, opt.verbose);
        }

        // Retrieve and delete Windows profile and folder if -r was supplied
        if (opt.removeHomeAndProfile) {
            DeleteUserProfile(opt.username, opt.verbose);
            DeleteHomeDirectory(opt.username, opt.rootDir, opt.verbose);
        }

        // Remove the user account from the Windows SAM database
        NET_API_STATUS nStatus = NetUserDel(NULL, wUsername.c_str());

        if (nStatus != NERR_Success) {
            errorMessage = FormatNetError(nStatus);
            return false;
        }

        return true;
    }

std::string WindowsUserManager::FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserNotFound: return "User account does not exist.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this command in an elevated administrator terminal.";
        case ERROR_INVALID_PARAMETER: return "Invalid parameter specified.";
        default: return "NetAPI Error code: " + std::to_string(status);
        }
    }
