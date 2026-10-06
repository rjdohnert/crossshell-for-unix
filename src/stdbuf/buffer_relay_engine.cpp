#include "buffer_config.hpp"
#include "buffer_relay_engine.hpp"

void BufferRelayEngine::relayOutput(HANDLE hReadPipe, HANDLE hParentWrite, BufferConfig config) {
        char buffer[4096];
        DWORD bytesRead = 0;
        std::vector<char> lineBuffer;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            if (config.mode == BufferMode::Unbuffered) {
                DWORD bytesWritten = 0;
                WriteFile(hParentWrite, buffer, bytesRead, &bytesWritten, NULL);
                FlushFileBuffers(hParentWrite);
            } else if (config.mode == BufferMode::LineBuffered) {
                for (DWORD i = 0; i < bytesRead; ++i) {
                    lineBuffer.push_back(buffer[i]);
                    if (buffer[i] == '\n') {
                        DWORD bytesWritten = 0;
                        WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(lineBuffer.size()), &bytesWritten, NULL);
                        FlushFileBuffers(hParentWrite);
                        lineBuffer.clear();
                    }
                }
            } else {
                lineBuffer.insert(lineBuffer.end(), buffer, buffer + bytesRead);
                while (lineBuffer.size() >= config.size) {
                    DWORD bytesWritten = 0;
                    WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(config.size), &bytesWritten, NULL);
                    FlushFileBuffers(hParentWrite);
                    lineBuffer.erase(lineBuffer.begin(), lineBuffer.begin() + config.size);
                }
            }
        }

        if (!lineBuffer.empty()) {
            DWORD bytesWritten = 0;
            WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(lineBuffer.size()), &bytesWritten, NULL);
            FlushFileBuffers(hParentWrite);
        }
    }
