#include "engine.hpp"

// ============================================================================
// EncodingConverter Implementation
// ============================================================================

std::wstring EncodingConverter::ConvertToWString(const std::vector<uint8_t>& raw_bytes) {
    if (raw_bytes.empty()) return L"";

    const uint8_t* data = raw_bytes.data();
    size_t size = raw_bytes.size();

    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        data += 3;
        size -= 3;
    }

    if (size == 0) return L"";

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
    if (wlen > 0) {
        std::wstring wstr(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), &wstr[0], wlen);
        return wstr;
    }

    wlen = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
    if (wlen > 0) {
        std::wstring wstr(wlen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), &wstr[0], wlen);
        return wstr;
    }

    return L"";
}

// ============================================================================
// StreamReader Implementation
// ============================================================================

std::vector<uint8_t> StreamReader::ReadAllStdin() {
    _setmode(_fileno(stdin), _O_BINARY);

    std::vector<uint8_t> buffer;
    constexpr size_t CHUNK_SIZE = 65536;
    uint8_t chunk[CHUNK_SIZE];

    while (true) {
        size_t bytes_read = fread(chunk, 1, CHUNK_SIZE, stdin);
        if (bytes_read == 0) break;
        buffer.insert(buffer.end(), chunk, chunk + bytes_read);
    }

    return buffer;
}

// ============================================================================
// ClipboardManager Implementation
// ============================================================================

bool ClipboardManager::SetClipboardText(const std::wstring& text) {
    constexpr int MAX_RETRIES = 10;
    constexpr int RETRY_DELAY_MS = 20;

    bool opened = false;
    for (int i = 0; i < MAX_RETRIES; ++i) {
        if (OpenClipboard(nullptr)) {
            opened = true;
            break;
        }
        Sleep(RETRY_DELAY_MS);
    }

    if (!opened) {
        std::cerr << "pbcopy: Error: Could not open Windows Clipboard (locked by another process).\n";
        return false;
    }

    if (!EmptyClipboard()) {
        std::cerr << "pbcopy: Error: Could not clear Windows Clipboard.\n";
        CloseClipboard();
        return false;
    }

    size_t bytes_needed = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes_needed);
    if (!hMem) {
        std::cerr << "pbcopy: Error: Out of memory for clipboard allocation.\n";
        CloseClipboard();
        return false;
    }

    wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!pMem) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    memcpy(pMem, text.c_str(), bytes_needed);
    GlobalUnlock(hMem);

    if (SetClipboardData(CF_UNICODETEXT, hMem) == nullptr) {
        std::cerr << "pbcopy: Error: Failed to set clipboard data.\n";
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}
