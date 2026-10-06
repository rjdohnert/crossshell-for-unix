#pragma once

#include "vhdctl.hpp"

class VhdStorageInspector {
public:
    static std::wstring CsvQuote(const std::wstring& value);

    static std::wstring GetErrorMessage(DWORD errorCode);

    static bool IsAdmin();

    static ULONG GetDeviceType(const std::wstring& path);

    static ULONGLONG ParseSize(const std::wstring& str);
};
