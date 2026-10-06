#include "pane.hpp"

Pane::Pane(int id, const std::string& title, std::function<void()> dirtyCb)
        : id(id), title(title), screen(80, 24), onDirty(dirtyCb) {
        // Direct feedback response pipeline for DA1/DA2/DSR queries
        screen.sendResponseCallback = [this](const std::string& resp) {
            SendInput(resp);
        };
    }

Pane::~Pane() {
        alive = false;
        if (hProcess != INVALID_HANDLE_VALUE) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            hProcess = INVALID_HANDLE_VALUE;
        }
        if (hPC != INVALID_HANDLE_VALUE) {
            ClosePseudoConsole(hPC);
            hPC = INVALID_HANDLE_VALUE;
        }
        if (hPipeIn != INVALID_HANDLE_VALUE) {
            CancelIoEx(hPipeIn, NULL);
            CloseHandle(hPipeIn);
            hPipeIn = INVALID_HANDLE_VALUE;
        }
        if (hPipeOut != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipeOut);
            hPipeOut = INVALID_HANDLE_VALUE;
        }
        if (readerThread.joinable()) {
            readerThread.join();
        }
    }

bool Pane::Start(const std::wstring& cmd, int w, int h) {
        bounds.width = w;
        bounds.height = h;
        screen.ResizeGrid(std::max(1, w), std::max(1, h));

        HANDLE hPipePTYIn, hPipePTYOut;
        if (!CreatePipe(&hPipePTYIn, &hPipeOut, NULL, 0)) return false;
        if (!CreatePipe(&hPipeIn, &hPipePTYOut, NULL, 0)) return false;

        COORD size = { (SHORT)screen.width, (SHORT)screen.height };
        HRESULT hr = CreatePseudoConsole(size, hPipePTYIn, hPipePTYOut, 0, &hPC);
        CloseHandle(hPipePTYIn);
        CloseHandle(hPipePTYOut);
        if (FAILED(hr)) return false;

        STARTUPINFOEXW siEx = { 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
        siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);
        InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &bytes);
        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL);

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back(0);

        BOOL success = CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siEx.StartupInfo, &pi);
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        free(siEx.lpAttributeList);
        if (!success) return false;

        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        readerThread = std::thread([this]() {
            char buffer[8192];
            DWORD read;
            while (alive && ReadFile(hPipeIn, buffer, sizeof(buffer), &read, NULL) && read > 0) {
                screen.ProcessBytes(buffer, read);
                if (onDirty) onDirty();
            }
            alive = false;
            if (onDirty) onDirty();
        });

        return true;
    }

void Pane::Resize(int w, int h) {
        bounds.width = w;
        bounds.height = h;
        screen.ResizeGrid(std::max(1, w), std::max(1, h));
        if (hPC != INVALID_HANDLE_VALUE) {
            COORD size = { (SHORT)screen.width, (SHORT)screen.height };
            ResizePseudoConsole(hPC, size);
        }
    }

void Pane::SendInput(const std::string& data) {
        if (hPipeOut != INVALID_HANDLE_VALUE && !data.empty()) {
            DWORD written;
            WriteFile(hPipeOut, data.data(), (DWORD)data.size(), &written, NULL);
        }
    }
