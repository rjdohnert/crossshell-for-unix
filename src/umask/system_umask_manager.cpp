#include "scoped_process_handle.hpp"
#include "scoped_registry_key.hpp"
#include "system_umask_manager.hpp"
#include "umask_formatter.hpp"
#include "umask_parser.hpp"

unsigned int SystemUmaskManager::GetSystemUmask() {
        wchar_t envBuf[32];
        DWORD res = GetEnvironmentVariableW(L"UMASK", envBuf, 32);
        if (res > 0 && res < 32) {
            std::string str = UmaskFormatter::WideToUtf8(std::wstring(envBuf));
            try {
                return UmaskParser::ParseMaskInput(DEFAULT_MASK, str);
            } catch (...) {}
        }

        ScopedRegistryKey hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, hKey.Receive()) == ERROR_SUCCESS) {
            wchar_t regBuf[32];
            DWORD dwType = REG_SZ;
            DWORD dwSize = sizeof(regBuf);
            if (RegQueryValueExW(hKey.Get(), L"UMASK", NULL, &dwType, reinterpret_cast<LPBYTE>(regBuf), &dwSize) == ERROR_SUCCESS) {
                std::string str = UmaskFormatter::WideToUtf8(std::wstring(regBuf));
                try {
                    return UmaskParser::ParseMaskInput(DEFAULT_MASK, str);
                } catch (...) {}
            }
        }

        return DEFAULT_MASK;
    }

void SystemUmaskManager::SetSystemUmask(unsigned int mask) {
        mask &= MASK_ALL_BITS;

        _umask(static_cast<int>(mask));

        std::string octalStr = UmaskFormatter::FormatOctalMask(mask);
        std::wstring wOctalStr(octalStr.begin(), octalStr.end());
        SetEnvironmentVariableW(L"UMASK", wOctalStr.c_str());

        ScopedRegistryKey hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, hKey.Receive()) == ERROR_SUCCESS) {
            RegSetValueExW(hKey.Get(), L"UMASK", 0, REG_SZ, 
                           reinterpret_cast<const BYTE*>(wOctalStr.c_str()), 
                           static_cast<DWORD>((wOctalStr.length() + 1) * sizeof(wchar_t)));

            DWORD_PTR dwResult = 0;
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                                reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 2000, &dwResult);
        }
    }

int SystemUmaskManager::ExecuteSubcommand(const std::wstring& cmdline) {
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        if (!CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "umask: failed to execute subcommand (Error " << GetLastError() << ")\n";
            return 1;
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess.Get(), &exitCode);

        return static_cast<int>(exitCode);
    }
