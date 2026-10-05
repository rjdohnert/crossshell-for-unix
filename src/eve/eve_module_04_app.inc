static bool SafePipeWrite(const std::string& commandLine, const std::string& inputData) {
    if (commandLine.empty()) return false;

    HANDLE hChildStdInRead = NULL;
    HANDLE hChildStdInWrite = NULL;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdInRead, &hChildStdInWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdInRead);
        CloseHandle(hChildStdInWrite);
        return false;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = hChildStdInRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::vector<char> cmdBuf(commandLine.begin(), commandLine.end());
    cmdBuf.push_back('\0');

    BOOL success = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    CloseHandle(hChildStdInRead);

    if (!success) {
        CloseHandle(hChildStdInWrite);
        return false;
    }

    DWORD written = 0;
    WriteFile(hChildStdInWrite, inputData.data(), static_cast<DWORD>(inputData.size()), &written, NULL);
    CloseHandle(hChildStdInWrite);

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(piProcInfo.hProcess, &exitCode);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return exitCode == 0;
}

static int eve_main(int argc, char* argv[]) {
    std::vector<std::string> files;
    bool read_only = false;
    int format = 0;
    std::string pipeCommand;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::string upper_arg = arg;
        std::transform(upper_arg.begin(), upper_arg.end(), upper_arg.begin(), ::toupper);
        if (upper_arg == "/JSON") { format = 1; continue; }
        if (upper_arg == "/CSV") { format = 2; continue; }
        if (upper_arg == "/TABLE") { format = 3; continue; }
        if (upper_arg == "/PIPE" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (upper_arg == "/READ_ONLY" || upper_arg == "/NOWRITE" || upper_arg == "-R") {
            read_only = true;
        } else if (arg[0] != '/' && arg[0] != '-') {
            files.push_back(argv[i]);
        }
    }
    EveEditor editor(files, read_only);
    editor.Run();
    if (format || !pipeCommand.empty()) {
        std::string text = format == 1 ? "{\"status\":\"completed\",\"files\":" + std::to_string(files.size()) + "}\n" : format == 2 ? "status,files\ncompleted," + std::to_string(files.size()) + "\n" : "STATUS\tFILES\ncompleted\t" + std::to_string(files.size()) + "\n";
        if (!pipeCommand.empty()) {
            if (!SafePipeWrite(pipeCommand, text)) return 1;
        } else {
            std::cout << text;
        }
    }
    return 0;
}

class EveApplication { public: int run(int argc, char* argv[]) const { return eve_main(argc, argv); } };
