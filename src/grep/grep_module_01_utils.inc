// ============================================================================
// 1. STRING UTILS & WINDOWS SYSTEM OBJECT LOADER
// ============================================================================

class GrepStringUtils {
public:
    static size_t TopLevelOperator(const std::string& expression, const std::string& op) {
        int depth = 0;
        char quote = '\0';
        for (size_t i = 0; i + op.size() <= expression.size(); ++i) {
            char ch = expression[i];
            if (quote != '\0') {
                if (ch == quote) {
                    size_t slashes = 0, k = i;
                    while (k > 0 && expression[k - 1] == '\\') { ++slashes; --k; }
                    if ((slashes % 2) == 0) quote = '\0';
                }
                continue;
            }
            if (ch == '\'' || ch == '"') { quote = ch; continue; }
            if (ch == '(') { ++depth; continue; }
            if (ch == ')' && depth > 0) { --depth; continue; }
            if (depth == 0 && expression.compare(i, op.size(), op) == 0) return i;
        }
        return std::string::npos;
    }

    static std::string Trim(const std::string& value) {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (unsigned char ch : value) {
            if (ch == '"') out += "\\\"";
            else if (ch == '\\') out += "\\\\";
            else if (ch == '\n') out += "\\n";
            else if (ch == '\r') out += "\\r";
            else if (ch == '\t') out += "\\t";
            else if (ch < 0x20) out += ' ';
            else out += static_cast<char>(ch);
        }
        return out;
    }

#ifdef _WIN32
    static std::string WideUtf8(const wchar_t* value) {
        if (!value) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) return {};
        std::string out(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
        return out;
    }

    static std::string VariantJson(const VARIANT& value) {
        VARIANT converted;
        VariantInit(&converted);
        if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(&value), 0, VT_BSTR))) return "null";
        std::string out = "\"" + JsonEscape(WideUtf8(converted.bstrVal)) + "\"";
        VariantClear(&converted);
        return out;
    }
#endif
};

class GrepWindowsObjectLoader {
public:
#ifdef _WIN32
    static bool LoadRegistry(const std::string& spec, std::string& output, std::string& error) {
        size_t slash = spec.find('\\');
        std::string rootName = slash == std::string::npos ? spec : spec.substr(0, slash);
        std::string subKey = slash == std::string::npos ? "" : spec.substr(slash + 1);
        HKEY root = nullptr;
        if (_stricmp(rootName.c_str(), "HKLM") == 0 || _stricmp(rootName.c_str(), "HKEY_LOCAL_MACHINE") == 0) root = HKEY_LOCAL_MACHINE;
        else if (_stricmp(rootName.c_str(), "HKCU") == 0 || _stricmp(rootName.c_str(), "HKEY_CURRENT_USER") == 0) root = HKEY_CURRENT_USER;
        else if (_stricmp(rootName.c_str(), "HKCR") == 0 || _stricmp(rootName.c_str(), "HKEY_CLASSES_ROOT") == 0) root = HKEY_CLASSES_ROOT;
        else if (_stricmp(rootName.c_str(), "HKU") == 0 || _stricmp(rootName.c_str(), "HKEY_USERS") == 0) root = HKEY_USERS;
        else { error = "invalid registry root '" + rootName + "'"; return false; }
        HKEY key = nullptr;
        if (RegOpenKeyExA(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) { error = "cannot open registry key '" + spec + "'"; return false; }
        for (DWORD index = 0;; ++index) {
            wchar_t name[16384] = {};
            DWORD nameSize = static_cast<DWORD>(sizeof(name) / sizeof(name[0])), type = 0, dataSize = 65536;
            BYTE data[65536] = {};
            LONG status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) continue;
            std::string value;
            if (type == REG_DWORD && dataSize >= sizeof(DWORD)) value = std::to_string(*reinterpret_cast<DWORD*>(data));
            else if (type == REG_QWORD && dataSize >= sizeof(ULONGLONG)) value = std::to_string(*reinterpret_cast<ULONGLONG*>(data));
            else if (type == REG_SZ || type == REG_EXPAND_SZ) {
                size_t chars = dataSize / sizeof(wchar_t);
                std::wstring wide(reinterpret_cast<const wchar_t*>(data), chars);
                if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
                value = GrepStringUtils::WideUtf8(wide.c_str());
            }
            else value.assign(reinterpret_cast<char*>(data), dataSize);
            output += "{\"name\":\"" + GrepStringUtils::JsonEscape(GrepStringUtils::WideUtf8(name)) + "\",\"value\":\"" + GrepStringUtils::JsonEscape(value) + "\"}\n";
        }
        RegCloseKey(key);
        return true;
    }

    static bool LoadWmi(const std::string& spec, std::string& output, std::string& error) {
        size_t pipe = spec.find('|');
        std::string query = pipe == std::string::npos ? spec : spec.substr(0, pipe);
        std::wstring ns = L"ROOT\\CIMV2";
        if (pipe != std::string::npos) {
            std::string text = spec.substr(pipe + 1);
            int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            ns.resize(n > 0 ? n - 1 : 0);
            if (n > 0) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, ns.data(), n);
        }
        HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool uninit = SUCCEEDED(init);
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) { error = "cannot initialize COM for WMI"; return false; }
        IWbemLocator* locator = nullptr; IWbemServices* services = nullptr; IEnumWbemClassObject* rows = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (SUCCEEDED(hr)) hr = locator->ConnectServer(_bstr_t(ns.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        if (SUCCEEDED(hr)) hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        int querySize = MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), nullptr, 0);
        std::wstring wideQuery(querySize, L'\0');
        if (querySize > 0) MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), wideQuery.data(), querySize);
        if (SUCCEEDED(hr)) hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wideQuery.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &rows);
        if (SUCCEEDED(hr)) {
            IWbemClassObject* row = nullptr; ULONG count = 0;
            while (rows->Next(WBEM_INFINITE, 1, &row, &count) == S_OK && count == 1) {
                SAFEARRAY* names = nullptr;
                row->GetNames(nullptr, WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names);
                output += "{";
                if (names) {
                    LONG lo = 0, hi = -1;
                    SafeArrayGetLBound(names, 1, &lo);
                    SafeArrayGetUBound(names, 1, &hi);
                    for (LONG i = lo; i <= hi; ++i) {
                        BSTR property = nullptr;
                        SafeArrayGetElement(names, &i, &property);
                        VARIANT value;
                        VariantInit(&value);
                        row->Get(property, 0, &value, nullptr, nullptr);
                        if (i > lo) output += ",";
                        output += "\"" + GrepStringUtils::JsonEscape(GrepStringUtils::WideUtf8(property)) + "\":" + GrepStringUtils::VariantJson(value);
                        VariantClear(&value);
                        SysFreeString(property);
                    }
                    SafeArrayDestroy(names);
                }
                output += "}\n";
                row->Release();
            }
        }
        if (rows) rows->Release(); if (services) services->Release(); if (locator) locator->Release(); if (uninit) CoUninitialize();
        if (FAILED(hr)) { error = "WMI query failed"; return false; }
        return true;
    }
#endif

    static bool LoadObjectInput(const std::string& source, std::string& output, std::string& error) {
#ifdef _WIN32
        if (source.rfind("registry:", 0) == 0) return LoadRegistry(source.substr(9), output, error);
        return LoadWmi(source.substr(4), output, error);
#else
        error = "registry and WMI input require Windows"; return false;
#endif
    }
};

