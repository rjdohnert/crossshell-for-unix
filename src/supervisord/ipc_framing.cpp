#include "ipc_framing.hpp"
#include "supervisor_defaults.hpp"

bool ReadExact(HANDLE h, char* buffer, DWORD bytesToRead) {
    DWORD total = 0;
    while (total < bytesToRead) {
        DWORD got = 0;
        if (!ReadFile(h, buffer + total, bytesToRead - total, &got, NULL) || got == 0) return false;
        total += got;
    }
    return true;
}

bool WriteExact(HANDLE h, const char* buffer, DWORD bytesToWrite) {
    DWORD total = 0;
    while (total < bytesToWrite) {
        DWORD written = 0;
        if (!WriteFile(h, buffer + total, bytesToWrite - total, &written, NULL) || written == 0) return false;
        total += written;
    }
    return true;
}

bool ReadIpcFrame(HANDLE h, std::string& payload) {
    std::string header;
    std::string preloadedPayload;
    constexpr size_t kMaxHeaderBytes = 127;
    std::array<char, 128> ioBuf{};

    while (true) {
        DWORD got = 0;
        if (!ReadFile(h, ioBuf.data(), static_cast<DWORD>(ioBuf.size()), &got, NULL) || got == 0) return false;

        const char* begin = ioBuf.data();
        const char* end = begin + got;
        const char* newline = std::find(begin, end, '\n');

        if (newline != end) {
            header.append(begin, newline);
            if (header.size() > kMaxHeaderBytes) return false;

            const char* payloadStart = newline + 1;
            if (payloadStart < end) {
                preloadedPayload.append(payloadStart, end);
            }
            break;
        }

        header.append(begin, end);
        if (header.size() > kMaxHeaderBytes) return false;
    }

    if (header.rfind("LEN:", 0) != 0) return false;
    DWORD len = 0;
    try {
        len = static_cast<DWORD>(std::stoul(header.substr(4)));
    } catch (...) {
        return false;
    }
    if (len > MAX_IPC_FRAME_PAYLOAD) return false;

    if (preloadedPayload.size() > len) return false;

    payload.resize(len);
    if (len == 0) return true;

    if (!preloadedPayload.empty()) {
        memcpy(payload.data(), preloadedPayload.data(), preloadedPayload.size());
    }

    DWORD remaining = len - static_cast<DWORD>(preloadedPayload.size());
    if (remaining == 0) return true;
    return ReadExact(h, payload.data() + preloadedPayload.size(), remaining);
}

bool WriteIpcFrame(HANDLE h, const std::string& payload) {
    if (payload.size() > MAX_IPC_FRAME_PAYLOAD) return false;
    std::string header = "LEN:" + std::to_string(payload.size()) + "\n";
    if (!WriteExact(h, header.data(), static_cast<DWORD>(header.size()))) return false;
    if (payload.empty()) return true;
    return WriteExact(h, payload.data(), static_cast<DWORD>(payload.size()));
}
