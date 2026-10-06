#include "stream_data.hpp"

void StreamData(HANDLE hRead, HANDLE hWrite) {
    char buffer[4096];
    DWORD bytesRead = 0, bytesWritten = 0;
    while (ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        if (!WriteFile(hWrite, buffer, bytesRead, &bytesWritten, NULL)) {
            break;
        }
    }
}
