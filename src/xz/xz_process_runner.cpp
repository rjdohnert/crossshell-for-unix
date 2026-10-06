#include "backend_config.hpp"
#include "xz_process_runner.hpp"

std::wstring XzProcessRunner::quoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

int XzProcessRunner::execute(const BackendConfig& backend, const std::vector<std::wstring>& userArgs) {
        std::vector<std::wstring> fullArgs;
        fullArgs.push_back(backend.applicationPath);
        for (const auto& a : backend.fixedArgs) fullArgs.push_back(a);
        for (const auto& a : userArgs) fullArgs.push_back(a);

        std::wstring cmdline;
        for (size_t i = 0; i < fullArgs.size(); ++i) {
            if (i > 0) cmdline.push_back(L' ');
            cmdline += quoteArgument(fullArgs[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
        if (!ok) {
            std::wcerr << L"xz: failed to execute backend: " << backend.applicationPath << L"\n";
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
