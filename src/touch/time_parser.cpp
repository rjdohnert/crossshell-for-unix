#include "time_parser.hpp"

bool TimeParser::IsDigits(const std::wstring& s) {
        return std::all_of(s.begin(), s.end(), [](wchar_t c) { return std::iswdigit(c); });
    }

bool TimeParser::ParseTimeSpec(const std::wstring& t_str, FILETIME& out_ft) {
        std::wstring clean_str = t_str;
        std::wstring ss_str = L"00";

        size_t dot_pos = t_str.find(L'.');
        if (dot_pos != std::wstring::npos) {
            ss_str = t_str.substr(dot_pos + 1);
            clean_str = t_str.substr(0, dot_pos);
        }

        if (ss_str.length() != 2 || !IsDigits(ss_str) || !IsDigits(clean_str)) {
            return false;
        }

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = std::stoi(ss_str);

        SYSTEMTIME current_st;
        GetLocalTime(&current_st);

        size_t len = clean_str.length();
        if (len == 8) { // MMDDhhmm
            year = current_st.wYear;
            month = std::stoi(clean_str.substr(0, 2));
            day = std::stoi(clean_str.substr(2, 2));
            hour = std::stoi(clean_str.substr(4, 2));
            minute = std::stoi(clean_str.substr(6, 2));
        } else if (len == 10) { // YYMMDDhhmm
            int yy = std::stoi(clean_str.substr(0, 2));
            year = (yy < 69) ? (2000 + yy) : (1900 + yy);
            month = std::stoi(clean_str.substr(2, 2));
            day = std::stoi(clean_str.substr(4, 2));
            hour = std::stoi(clean_str.substr(6, 2));
            minute = std::stoi(clean_str.substr(8, 2));
        } else if (len == 12) { // CCYYMMDDhhmm
            year = std::stoi(clean_str.substr(0, 4));
            month = std::stoi(clean_str.substr(4, 2));
            day = std::stoi(clean_str.substr(6, 2));
            hour = std::stoi(clean_str.substr(8, 2));
            minute = std::stoi(clean_str.substr(10, 2));
        } else {
            return false;
        }

        SYSTEMTIME local_st = { 0 };
        local_st.wYear = static_cast<WORD>(year);
        local_st.wMonth = static_cast<WORD>(month);
        local_st.wDay = static_cast<WORD>(day);
        local_st.wHour = static_cast<WORD>(hour);
        local_st.wMinute = static_cast<WORD>(minute);
        local_st.wSecond = static_cast<WORD>(second);

        SYSTEMTIME utc_st;
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &local_st, &utc_st)) {
            return false;
        }

        return SystemTimeToFileTime(&utc_st, &out_ft) != 0;
    }

bool TimeParser::GetReferenceTimes(const std::wstring& ref_path, FILETIME& out_at, FILETIME& out_mt) {
        HANDLE hFile = CreateFileW(
            ref_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        FILETIME ct;
        bool success = GetFileTime(hFile, &ct, &out_at, &out_mt) != 0;
        CloseHandle(hFile);
        return success;
    }
