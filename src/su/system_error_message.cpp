#include "system_error_message.hpp"

std::wstring GetSystemErrorMessage(DWORD errorCode) {
    LPWSTR buf = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buf, 0, NULL
    );
    std::wstring msg = (size && buf) ? buf : L"Unknown error.";
    if (buf) LocalFree(buf);

    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) {
        msg.pop_back();
    }
    return msg;
}
