#include "yes_engine.hpp"

BOOL WINAPI YesEngine::consoleControlHandler(DWORD controlType) {
        if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT) {
            InterlockedExchange(&stopRequested, 1);
            return TRUE;
        }
        return FALSE;
    }

int YesEngine::execute(const std::string& inputLine) {
        std::string line = inputLine + "\n";

        std::vector<char> buffer;
        if (line.size() < BUFFER_SIZE) {
            buffer.reserve(BUFFER_SIZE);
            while (buffer.size() + line.size() <= BUFFER_SIZE) {
                buffer.insert(buffer.end(), line.begin(), line.end());
            }
        }

        const char* writePtr = buffer.empty() ? line.c_str() : buffer.data();
        DWORD writeSize = buffer.empty() ? static_cast<DWORD>(line.size()) : static_cast<DWORD>(buffer.size());

        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout == INVALID_HANDLE_VALUE || hStdout == NULL) {
            std::cerr << "yes: unable to access standard output handle\n";
            return 1;
        }

        if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE)) {
            std::cerr << "yes: unable to install console control handler\n";
            return 1;
        }

        DWORD bytesWritten = 0;
        while (InterlockedCompareExchange(&stopRequested, 0, 0) == 0) {
            BOOL result = WriteFile(hStdout, writePtr, writeSize, &bytesWritten, NULL);

            if (InterlockedCompareExchange(&stopRequested, 0, 0) != 0) {
                break;
            }

            if (!result) {
                DWORD err = GetLastError();
                if (err == ERROR_NO_DATA || err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF) {
                    break;
                }
                break;
            }
        }

        SetConsoleCtrlHandler(consoleControlHandler, FALSE);
        return 0;
    }
