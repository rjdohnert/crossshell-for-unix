    // =========================================================================
    // String & Security Descriptor RAII Helpers
    // =========================================================================
    class Win32Helper {
    public:
        static std::string wide_to_utf8(std::wstring_view wstr) {
            if (wstr.empty()) return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string result(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        static std::wstring utf8_to_wide(std::string_view str) {
            if (str.empty()) return {};
            int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring result(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
            return result;
        }

        static std::string sid_to_string(PSID pSid) {
            if (!pSid || !IsValidSid(pSid)) return "UNKNOWN_SID";
            LPWSTR str_sid = nullptr;
            if (ConvertSidToStringSidW(pSid, &str_sid) && str_sid) {
                std::string result = wide_to_utf8(str_sid);
                LocalFree(str_sid);
                return result;
            }
            return "INVALID_SID";
        }

        static std::string resolve_sid_to_name(PSID pSid, bool numeric, SID_NAME_USE* out_type = nullptr) {
            if (!pSid || !IsValidSid(pSid)) return "UNKNOWN";
            if (numeric) return sid_to_string(pSid);

            wchar_t name[256];
            DWORD name_len = 256;
            wchar_t domain[256];
            DWORD domain_len = 256;
            SID_NAME_USE use;

            if (LookupAccountSidW(nullptr, pSid, name, &name_len, domain, &domain_len, &use)) {
                if (out_type) *out_type = use;
                std::string resolved_name = wide_to_utf8(name);
                std::string resolved_domain = wide_to_utf8(domain);
                if (!resolved_domain.empty()) {
                    return resolved_domain + "\\" + resolved_name;
                }
                return resolved_name;
            }

            return sid_to_string(pSid);
        }

        static std::string mask_to_rwx(DWORD mask) {
            std::string perms = "---";
            // Check Read
            if ((mask & FILE_GENERIC_READ) == FILE_GENERIC_READ || (mask & FILE_READ_DATA) || (mask & GENERIC_READ) || (mask & GENERIC_ALL)) {
                perms[0] = 'r';
            }
            // Check Write
            if ((mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE || (mask & FILE_WRITE_DATA) || (mask & GENERIC_WRITE) || (mask & GENERIC_ALL)) {
                perms[1] = 'w';
            }
            // Check Execute
            if ((mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE || (mask & FILE_EXECUTE) || (mask & GENERIC_EXECUTE) || (mask & GENERIC_ALL)) {
                perms[2] = 'x';
            }
            return perms;
        }
    };

