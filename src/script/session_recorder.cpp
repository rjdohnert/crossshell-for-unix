#include "session_recorder.hpp"

std::wstring CommandLineBuilder::quoteArgument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }

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

std::wstring CommandLineBuilder::build(const std::vector<std::wstring>& args) {
    std::wstring out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            out.push_back(L' ');
        }
        out += quoteArgument(args[i]);
    }
    return out;
}

int ScriptEngine::execute(const wchar_t* progName) {
    (void)progName;
    HANDLE fileHandle = CreateFileW(
        options.outputFile.c_str(),
        FILE_GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        options.append ? OPEN_ALWAYS : CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        std::wcerr << L"script: cannot open output file: " << options.outputFile << L"\n";
        return 1;
    }

    if (options.append) {
        SetFilePointer(fileHandle, 0, nullptr, FILE_END);
    }

    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);

    HANDLE pipeRead = nullptr;
    HANDLE pipeWrite = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&pipeRead, &pipeWrite, &sa, 0)) {
        std::wcerr << L"script: failed to create output pipe\n";
        CloseHandle(fileHandle);
        return 1;
    }

    SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmdline;
    if (options.commandArgs.empty()) {
        wchar_t comspec[MAX_PATH];
        DWORD len = GetEnvironmentVariableW(L"COMSPEC", comspec, MAX_PATH);
        cmdline = (len > 0 && len < MAX_PATH) ? comspec : L"cmd.exe";
    } else {
        cmdline = CommandLineBuilder::build(options.commandArgs);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinHandle;
    si.hStdOutput = pipeWrite;
    si.hStdError = pipeWrite;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdBuffer(cmdline.begin(), cmdline.end());
    cmdBuffer.push_back(L'\0');

    std::wcout << L"Script started, file is " << options.outputFile << L"\n";
    TranscriptWriter::writeString(fileHandle, "Script started on Windows\r\n");

    BOOL created = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(pipeWrite);

    if (!created) {
        std::wcerr << L"script: failed to execute command: " << cmdline << L"\n";
        CloseHandle(pipeRead);
        CloseHandle(fileHandle);
        return 1;
    }

    BYTE buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(pipeRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        TranscriptWriter::writeAll(stdoutHandle, buffer, bytesRead);
        TranscriptWriter::writeAll(fileHandle, buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(pipeRead);

    TranscriptWriter::writeString(fileHandle, "\r\nScript done on Windows\r\n");
    CloseHandle(fileHandle);

    std::wcout << L"Script done, file is " << options.outputFile << L"\n";
    return static_cast<int>(exitCode);
}
