class DiskpartExecutor {
public:
    static bool ExecuteScript(const std::wstring& scriptContent) {
        WCHAR tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring scriptPath = std::wstring(tempPath) + L"fdisk_script.txt";

        ScopedFileHandle hFile(CreateFileW(scriptPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hFile.IsValid()) return false;

        int scriptBytes = WideCharToMultiByte(CP_ACP, 0, scriptContent.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (scriptBytes <= 0) {
            hFile.Close();
            DeleteFileW(scriptPath.c_str());
            return false;
        }

        std::string ansiScript(static_cast<size_t>(scriptBytes - 1), '\0');
        if (WideCharToMultiByte(CP_ACP, 0, scriptContent.c_str(), -1, ansiScript.data(), scriptBytes, nullptr, nullptr) <= 0) {
            hFile.Close();
            DeleteFileW(scriptPath.c_str());
            return false;
        }

        DWORD bytesWritten;
        WriteFile(hFile.Get(), ansiScript.c_str(), (DWORD)ansiScript.size(), &bytesWritten, NULL);
        hFile.Close();

        std::wstring command = L"diskpart.exe /s \"" + scriptPath + L"\"";
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        bool success = false;
        if (CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            success = (exitCode == 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        DeleteFileW(scriptPath.c_str());
        return success;
    }
};
