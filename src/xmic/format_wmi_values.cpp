#include "xmic_common.hpp"

std::wstring FormatBytes(unsigned long long bytes) {
    if (bytes == 0) return L"N/A";
    double gb = static_cast<double>(bytes) / (1024 * 1024 * 1024);
    wchar_t buf[32];
    if (gb >= 1.0) {
        swprintf_s(buf, L"%.2f GB", gb);
        return buf;
    }
    double mb = static_cast<double>(bytes) / (1024 * 1024);
    swprintf_s(buf, L"%.2f MB", mb);
    return buf;
}
bool TryParseUnsignedLongLong(const std::wstring& input, unsigned long long& valueOut) {
    valueOut = 0;
    if (input.empty()) {
        return false;
    }

    errno = 0;
    wchar_t* endPtr = nullptr;
    const unsigned long long parsed = _wcstoui64(input.c_str(), &endPtr, 10);
    if (endPtr == input.c_str() || *endPtr != L'\0' || errno == ERANGE) {
        return false;
    }

    valueOut = parsed;
    return true;
}
std::wstring FormatJsonFloatingToken(double value) {
    thread_local std::wostringstream stream;
    thread_local bool initialized = false;
    if (!initialized) {
        stream.imbue(std::locale::classic());
        initialized = true;
    }

    stream.str(L"");
    stream.clear();
    stream << value;
    return stream.str();
}
std::wstring VariantToString(VARIANT& vtProp) {
    if (V_VT(&vtProp) == VT_EMPTY || V_VT(&vtProp) == VT_NULL) return L"N/A";
    if (V_VT(&vtProp) == VT_BSTR && V_BSTR(&vtProp) != NULL) return std::wstring(V_BSTR(&vtProp));
    if (V_VT(&vtProp) == VT_I4) return std::to_wstring(V_I4(&vtProp));
    if (V_VT(&vtProp) == VT_UI4) return std::to_wstring(V_UI4(&vtProp));
    if (V_VT(&vtProp) == VT_UI8) return std::to_wstring(V_UI8(&vtProp));
    if (V_VT(&vtProp) == VT_I8) return std::to_wstring(V_I8(&vtProp));
    if (V_VT(&vtProp) == VT_BOOL) return (V_BOOL(&vtProp) ? L"True" : L"False");

    // Handle SafeArray of Strings (e.g., Network IPAddresses)
    if ((V_VT(&vtProp) & VT_ARRAY) && (V_VT(&vtProp) & VT_BSTR)) {
        SAFEARRAY* psa = V_ARRAY(&vtProp);
        BSTR* bstrData;
        std::wstring result = L"";
        if (SUCCEEDED(SafeArrayAccessData(psa, (void**)&bstrData))) {
            long lBound, uBound;
            SafeArrayGetLBound(psa, 1, &lBound);
            SafeArrayGetUBound(psa, 1, &uBound);
            
            std::vector<std::wstring> items;
            for (long i = lBound; i <= uBound; ++i) {
                long idx = i - lBound;
                if (bstrData[idx] != NULL) {
                    std::wstring str = bstrData[idx];
                    if (!str.empty()) items.push_back(str);
                }
            }
            SafeArrayUnaccessData(psa);

            // Put IPv4 before IPv6 addresses for cleaner network outputs
            std::stable_sort(items.begin(), items.end(), [](const std::wstring& a, const std::wstring& b) {
                bool a_has_colon = (a.find(L':') != std::wstring::npos);
                bool b_has_colon = (b.find(L':') != std::wstring::npos);
                return !a_has_colon && b_has_colon;
            });

            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) result += L", ";
                result += items[i];
            }
            return result.empty() ? L"N/A" : result;
        }
    }

    // Generic conversion attempt
    VARIANT dest;
    VariantInit(&dest);
    if (SUCCEEDED(VariantChangeType(&dest, &vtProp, 0, VT_BSTR))) {
        std::wstring str(V_BSTR(&dest) ? V_BSTR(&dest) : L"");
        VariantClear(&dest);
        return str;
    }
    return L"<Data>";
}
std::wstring FormatWmiDateTime(const std::wstring& wmiDateTime) {
    if (wmiDateTime.length() < 14) return wmiDateTime;

    std::wstring year = wmiDateTime.substr(0, 4);
    std::wstring month = wmiDateTime.substr(4, 2);
    std::wstring day = wmiDateTime.substr(6, 2);
    std::wstring hour = wmiDateTime.substr(8, 2);
    std::wstring minute = wmiDateTime.substr(10, 2);
    std::wstring second = wmiDateTime.substr(12, 2);

    return year + L"-" + month + L"-" + day + L" " + hour + L":" + minute + L":" + second;
}
std::wstring EscapeJson(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size() + 8);

    for (wchar_t ch : input) {
        switch (ch) {
        case L'\\': out += L"\\\\"; break;
        case L'\"': out += L"\\\""; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (ch >= 0 && ch <= 0x1F) {
                wchar_t buf[7];
                swprintf_s(buf, L"\\u%04X", static_cast<unsigned int>(ch));
                out += buf;
            } else {
                out += ch;
            }
            break;
        }
    }

    return out;
}
bool IsIntegerLiteral(const std::wstring& value) {
    if (value.empty()) return false;

    size_t start = 0;
    if (value[0] == L'-') {
        if (value.size() == 1) return false;
        start = 1;
    } else if (value[0] == L'+') {
        return false;
    }

    if (value[start] == L'0') {
        return (value.size() == start + 1);
    }

    if (value[start] < L'1' || value[start] > L'9') return false;

    for (size_t i = start + 1; i < value.size(); ++i) {
        if (!iswdigit(value[i])) return false;
    }

    return true;
}
std::wstring VariantToJson(VARIANT& vtProp) {
    if (V_VT(&vtProp) == VT_EMPTY || V_VT(&vtProp) == VT_NULL) {
        return L"null";
    }

    if (V_VT(&vtProp) & VT_ARRAY) {
        VARTYPE elementType = static_cast<VARTYPE>(V_VT(&vtProp) & VT_TYPEMASK);
        SAFEARRAY* psa = V_ARRAY(&vtProp);
        if (!psa) {
            return L"null";
        }

        long lBound = 0;
        long uBound = -1;
        if (FAILED(SafeArrayGetLBound(psa, 1, &lBound)) || FAILED(SafeArrayGetUBound(psa, 1, &uBound)) || uBound < lBound) {
            return L"[]";
        }

        std::wstring out;
        out.reserve(static_cast<size_t>(uBound - lBound + 1) * 10 + 2);
        out += L"[";
        bool first = true;
        for (long i = lBound; i <= uBound; ++i) {
            if (!first) out += L",";
            first = false;

            switch (elementType) {
            case VT_BSTR: {
                BSTR v = NULL;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v)) && v != NULL) {
                    out += L"\"";
                    out += EscapeJson(v);
                    out += L"\"";
                    SysFreeString(v);
                } else {
                    out += L"null";
                }
                break;
            }
            case VT_BOOL: {
                VARIANT_BOOL v = VARIANT_FALSE;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) {
                    out += (v == VARIANT_FALSE ? L"false" : L"true");
                } else {
                    out += L"null";
                }
                break;
            }
            case VT_I1: {
                CHAR v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(static_cast<int>(v)); else out += L"null";
                break;
            }
            case VT_UI1: {
                BYTE v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(static_cast<unsigned int>(v)); else out += L"null";
                break;
            }
            case VT_I2: {
                SHORT v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_UI2: {
                USHORT v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_I4: {
                LONG v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_UI4: {
                ULONG v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_I8: {
                LONGLONG v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_UI8: {
                ULONGLONG v = 0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) out += std::to_wstring(v); else out += L"null";
                break;
            }
            case VT_R4: {
                FLOAT v = 0.0f;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) {
                    out += FormatJsonFloatingToken(static_cast<double>(v));
                } else {
                    out += L"null";
                }
                break;
            }
            case VT_R8: {
                DOUBLE v = 0.0;
                if (SUCCEEDED(SafeArrayGetElement(psa, &i, &v))) {
                    out += FormatJsonFloatingToken(v);
                } else {
                    out += L"null";
                }
                break;
            }
            default: {
                out += L"null";
                break;
            }
            }
        }
        out += L"]";
        return out;
    }

    switch (V_VT(&vtProp)) {
    case VT_BOOL:
        return (V_BOOL(&vtProp) == VARIANT_FALSE) ? L"false" : L"true";
    case VT_I1:
        return std::to_wstring(static_cast<int>(V_I1(&vtProp)));
    case VT_UI1:
        return std::to_wstring(static_cast<unsigned int>(V_UI1(&vtProp)));
    case VT_I2:
        return std::to_wstring(V_I2(&vtProp));
    case VT_UI2:
        return std::to_wstring(V_UI2(&vtProp));
    case VT_I4:
    case VT_INT:
        return std::to_wstring(V_I4(&vtProp));
    case VT_UI4:
    case VT_UINT:
        return std::to_wstring(V_UI4(&vtProp));
    case VT_I8:
        return std::to_wstring(V_I8(&vtProp));
    case VT_UI8:
        return std::to_wstring(V_UI8(&vtProp));
    case VT_R4: {
        return FormatJsonFloatingToken(static_cast<double>(V_R4(&vtProp)));
    }
    case VT_R8: {
        return FormatJsonFloatingToken(V_R8(&vtProp));
    }
    case VT_BSTR: {
        std::wstring raw = V_BSTR(&vtProp) ? std::wstring(V_BSTR(&vtProp)) : L"";
        if (IsIntegerLiteral(raw)) {
            return raw;
        }

        std::wstring lowered = raw;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
        if (lowered == L"true") return L"true";
        if (lowered == L"false") return L"false";

        return L"\"" + EscapeJson(raw) + L"\"";
    }
    default:
        break;
    }

    std::wstring fallback = VariantToString(vtProp);
    return L"\"" + EscapeJson(fallback) + L"\"";
}
std::wstring EscapeCsv(const std::wstring& input) {
    bool needsQuotes = false;
    std::wstring normalized;
    normalized.reserve(input.size() + 8);

    for (wchar_t ch : input) {
        if (ch >= 0 && ch <= 0x1F) {
            wchar_t buf[5];
            swprintf_s(buf, L"\\x%02X", static_cast<unsigned int>(ch));
            normalized += buf;
            needsQuotes = true;
        } else {
            if (ch == L',' || ch == L'\"' || ch == L'\n' || ch == L'\r') {
                needsQuotes = true;
            }
            normalized += ch;
        }
    }

    if (!needsQuotes) return normalized;

    std::wstring out = L"\"";
    for (wchar_t ch : normalized) {
        if (ch == L'\"') out += L"\"\"";
        else out += ch;
    }
    out += L"\"";
    return out;
}
std::wstring EscapePowerShellSingleQuoted(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size() + 8);
    for (wchar_t ch : input) {
        if (ch == L'\'') {
            out += L"''";
        } else {
            out += ch;
        }
    }
    return out;
}
