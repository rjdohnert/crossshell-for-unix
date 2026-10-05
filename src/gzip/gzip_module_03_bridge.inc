// ============================================================================
// 3. COMPRESSION DISPATCH ENGINE
// ============================================================================

class PowerShellGzipBridge {
private:
    static std::wstring quoteArgument(const std::wstring& arg) {
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

    static std::wstring escapeSingleQuotes(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            if (c == L'\'') out += L"''";
            else out.push_back(c);
        }
        return out;
    }

    static int runPowerShell(const std::wstring& script) {
        std::vector<std::wstring> args = {
            L"powershell", L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-Command", script
        };

        std::wstring cmdline;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmdline.push_back(L' ');
            cmdline += quoteArgument(args[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
        if (!ok) return 1;

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }

public:
    static bool compressFile(const std::wstring& src, const std::wstring& dst) {
        std::wstring script =
            L"$in = [System.IO.File]::OpenRead('" + escapeSingleQuotes(src) + L"'); "
            L"$out = [System.IO.File]::Create('" + escapeSingleQuotes(dst) + L"'); "
            L"$gz = New-Object System.IO.Compression.GZipStream($out, [System.IO.Compression.CompressionMode]::Compress); "
            L"$in.CopyTo($gz); "
            L"$gz.Dispose(); $out.Dispose(); $in.Dispose();";
        return runPowerShell(script) == 0;
    }

    static bool decompressFile(const std::wstring& src, const std::wstring& dst) {
        std::wstring script =
            L"$in = [System.IO.File]::OpenRead('" + escapeSingleQuotes(src) + L"'); "
            L"$gz = New-Object System.IO.Compression.GZipStream($in, [System.IO.Compression.CompressionMode]::Decompress); "
            L"$out = [System.IO.File]::Create('" + escapeSingleQuotes(dst) + L"'); "
            L"$gz.CopyTo($out); "
            L"$out.Dispose(); $gz.Dispose(); $in.Dispose();";
        return runPowerShell(script) == 0;
    }
};

