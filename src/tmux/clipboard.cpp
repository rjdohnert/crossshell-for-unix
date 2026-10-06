#include "clipboard.hpp"

bool SetOSClipboard(const std::wstring& text) {
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    size_t sz = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.data(), sz);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    return true;
}

std::wstring GetOSClipboard() {
    if (!OpenClipboard(NULL)) return L"";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    std::wstring str;
    if (hData) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(hData));
        if (p) { str = p; GlobalUnlock(hData); }
    }
    CloseClipboard();
    return str;
}
