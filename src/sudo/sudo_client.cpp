#include "self_path.hpp"
#include "stream_data.hpp"
#include "sudo_client.hpp"

int RunClient(const std::wstring& targetCmd) {
    // Generate unique GUID for IPC session
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t guidBuf[64];
    StringFromGUID2(guid, guidBuf, 64);
    std::wstring guidStr(guidBuf);

    std::wstring pipeInName   = L"\\\\.\\pipe\\sudo_in_" + guidStr;
    std::wstring pipeOutName  = L"\\\\.\\pipe\\sudo_out_" + guidStr;
    std::wstring pipeErrName  = L"\\\\.\\pipe\\sudo_err_" + guidStr;
    std::wstring pipeExitName = L"\\\\.\\pipe\\sudo_exit_" + guidStr;

    // Build Security Descriptor allowing unprivileged to elevated pipe access
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE };

    // Create named pipes
    HANDLE hPipeIn = CreateNamedPipeW(pipeInName.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
    HANDLE hPipeOut = CreateNamedPipeW(pipeOutName.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
    HANDLE hPipeErr = CreateNamedPipeW(pipeErrName.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
    HANDLE hPipeExit = CreateNamedPipeW(pipeExitName.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);

    if (hPipeIn == INVALID_HANDLE_VALUE || hPipeOut == INVALID_HANDLE_VALUE ||
        hPipeErr == INVALID_HANDLE_VALUE || hPipeExit == INVALID_HANDLE_VALUE) {
        std::wcerr << L"sudo: failed to create IPC pipes.\n";
        return 1;
    }

    // Trigger UAC elevation for hidden worker
    std::wstring workerParams = L"--sudo-worker " + guidStr + L" " + targetCmd;
    std::wstring selfPath = GetSelfPath();

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = selfPath.c_str();
    sei.lpParameters = workerParams.c_str();
    sei.nShow = SW_HIDE; // Keeps worker hidden from creating a new window

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() == ERROR_CANCELLED) {
            std::wcerr << L"sudo: UAC prompt was denied.\n";
        } else {
            std::wcerr << L"sudo: elevation failed (Error " << GetLastError() << L")\n";
        }
        return 1;
    }

    // Await pipe connections from worker
    ConnectNamedPipe(hPipeIn, NULL);
    ConnectNamedPipe(hPipeOut, NULL);
    ConnectNamedPipe(hPipeErr, NULL);
    ConnectNamedPipe(hPipeExit, NULL);

    // Spawn async background streaming threads to relay I/O
    std::thread tOut(StreamData, hPipeOut, GetStdHandle(STD_OUTPUT_HANDLE));
    std::thread tErr(StreamData, hPipeErr, GetStdHandle(STD_ERROR_HANDLE));
    std::thread tIn(StreamData, GetStdHandle(STD_INPUT_HANDLE), hPipeIn);

    // Read exit code from elevated process
    DWORD exitCode = 1;
    DWORD bytesRead = 0;
    ReadFile(hPipeExit, &exitCode, sizeof(exitCode), &bytesRead, NULL);

    if (tOut.joinable()) tOut.join();
    if (tErr.joinable()) tErr.join();
    if (tIn.joinable()) tIn.detach(); // Detach stdin thread as it may block on ReadFile

    CloseHandle(hPipeIn);
    CloseHandle(hPipeOut);
    CloseHandle(hPipeErr);
    CloseHandle(hPipeExit);

    return static_cast<int>(exitCode);
}
