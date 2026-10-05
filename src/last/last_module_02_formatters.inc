
// Convert Wide String to UTF-8
std::string WStrToStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], size, NULL, NULL);
    return str;
}

// Safe XML Tag & Data Extractors
std::wstring ExtractXmlTag(const std::wstring& xml, const std::wstring& tag) {
    std::wstring openTag = L"<" + tag + L">";
    size_t start = xml.find(openTag);
    if (start == std::wstring::npos) return L"";
    start += openTag.length();
    size_t end = xml.find(L"</" + tag + L">", start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

std::wstring ExtractXmlAttr(const std::wstring& xml, const std::wstring& tag, const std::wstring& attr) {
    size_t tagPos = xml.find(L"<" + tag);
    if (tagPos == std::wstring::npos) return L"";

    std::wstring p1 = attr + L"='";
    std::wstring p2 = attr + L"=\"";

    size_t attrPos = xml.find(p1, tagPos);
    wchar_t quote = L'\'';
    size_t len = p1.length();

    if (attrPos == std::wstring::npos) {
        attrPos = xml.find(p2, tagPos);
        quote = L'"';
        len = p2.length();
    }
    if (attrPos == std::wstring::npos) return L"";

    size_t start = attrPos + len;
    size_t end = xml.find(quote, start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

std::wstring ExtractXmlData(const std::wstring& xml, const std::wstring& dataName) {
    std::wstring p1 = L"Name='" + dataName + L"'>";
    std::wstring p2 = L"Name=\"" + dataName + L"\">";

    size_t pos = xml.find(p1);
    size_t len = p1.length();
    if (pos == std::wstring::npos) {
        pos = xml.find(p2);
        len = p2.length();
    }
    if (pos == std::wstring::npos) return L"";

    size_t start = pos + len;
    size_t end = xml.find(L"</Data>", start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

// Convert ISO8601 XML SystemTime string to Timestamp
ULONGLONG ISO8601ToTimestamp(const std::wstring& isoStr) {
    if (isoStr.empty()) return 0;
    SYSTEMTIME st = { 0 };
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (swscanf_s(isoStr.c_str(), L"%d-%d-%dT%d:%d:%d",
        &year, &month, &day, &hour, &min, &sec) >= 6) {
        st.wYear = (WORD)year;
        st.wMonth = (WORD)month;
        st.wDay = (WORD)day;
        st.wHour = (WORD)hour;
        st.wMinute = (WORD)min;
        st.wSecond = (WORD)sec;

        FILETIME ftUtc;
        if (SystemTimeToFileTime(&st, &ftUtc)) {
            return ((ULONGLONG)ftUtc.dwHighDateTime << 32) | ftUtc.dwLowDateTime;
        }
    }
    return 0;
}

std::string FormatTime(ULONGLONG timestamp) {
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(timestamp & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(timestamp >> 32);

    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %2d %02d:%02d",
        days[stLocal.wDayOfWeek % 7],
        months[(stLocal.wMonth - 1) % 12],
        stLocal.wDay,
        stLocal.wHour,
        stLocal.wMinute);
    return buf;
}

std::string FormatDuration(ULONGLONG startFt, ULONGLONG endFt) {
    if (endFt <= startFt) return "(00:00)";
    ULONGLONG diffSec = (endFt - startFt) / 10000000ULL;

    ULONGLONG days = diffSec / 86400;
    ULONGLONG hours = (diffSec % 86400) / 3600;
    ULONGLONG minutes = (diffSec % 3600) / 60;

    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "(%llud+%02llu:%02llu)", days, hours, minutes);
    } else {
        snprintf(buf, sizeof(buf), "(%02llu:%02llu)", hours, minutes);
    }
    return buf;
}

std::wstring LogonTypeToTTY(DWORD type) {
    switch (type) {
    case 2:  return L"console";
    case 3:  return L"network";
    case 7:  return L"unlock";
    case 8:  return L"netclear";
    case 10: return L"pts/rdp";
    case 11: return L"cached";
    default: return L"session";
    }
}

bool IsSystemNoiseUser(const std::wstring& user) {
    if (user.empty()) return true;
    if (user.back() == L'$') return true; // Computer accounts

    static const std::unordered_set<std::wstring> noise = {
        L"SYSTEM", L"LOCAL SERVICE", L"NETWORK SERVICE", L"ANONYMOUS LOGON"
    };
    if (noise.count(user)) return true;
    if (user.rfind(L"DWM-", 0) == 0 || user.rfind(L"UMFD-", 0) == 0) return true;

    return false;
}
