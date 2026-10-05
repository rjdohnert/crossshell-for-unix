    // =========================================================================
    // Database: passwd (Local Users & Accounts)
    // Format: username:password:uid:gid:gecos:home_dir:shell
    // =========================================================================
    class PasswdHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "passwd"; }
        std::string description() const override { return "User account database (Win32 SAM / Local Users)"; }

        bool enumerate(std::ostream& out) override {
            LPUSER_INFO_2 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0, resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetUserEnum(nullptr, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&p_buf,
                                     MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<USER_INFO_2> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        print_user(out, safe_buf.get()[i]);
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::wstring wkey = StringHelper::utf8_to_wide(key);
            LPUSER_INFO_2 p_buf = nullptr;

            // Direct name lookup
            NET_API_STATUS status = NetUserGetInfo(nullptr, wkey.c_str(), 2, (LPBYTE*)&p_buf);
            NetApiPtr<USER_INFO_2> safe_buf(p_buf);

            if (status == NERR_Success && safe_buf) {
                print_user(out, *safe_buf);
                return true;
            }

            // Fallback: match by numeric UID (RID)
            try {
                DWORD target_rid = std::stoul(key);
                return enumerate_and_match_uid(target_rid, out);
            } catch (...) {
                return false;
            }
        }

    private:
        static DWORD get_rid_from_name(const std::wstring& username) {
            DWORD sid_size = 0, domain_size = 0;
            SID_NAME_USE use;
            LookupAccountNameW(nullptr, username.c_str(), nullptr, &sid_size, nullptr, &domain_size, &use);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> sid(sid_size);
                std::vector<wchar_t> domain(domain_size);
                if (LookupAccountNameW(nullptr, username.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use)) {
                    PUCHAR count = GetSidSubAuthorityCount(sid.data());
                    if (count && *count > 0) {
                        return *GetSidSubAuthority(sid.data(), *count - 1);
                    }
                }
            }
            return 1000;
        }

        static void print_user(std::ostream& out, const USER_INFO_2& usr) {
            std::wstring wname = usr.usri2_name ? usr.usri2_name : L"";
            std::string name = StringHelper::wide_to_utf8(wname);
            DWORD uid = get_rid_from_name(wname);
            DWORD gid = 513; // Standard Windows Primary Group (None / Domain Users)
            std::string gecos = usr.usri2_full_name ? StringHelper::wide_to_utf8(usr.usri2_full_name) : "";
            std::string home = usr.usri2_home_dir ? StringHelper::wide_to_utf8(usr.usri2_home_dir) : "";
            if (home.empty()) home = "C:\\Users\\" + name;
            std::string shell = "C:\\Windows\\System32\\cmd.exe";

            out << name << ":*:" << uid << ":" << gid << ":" << gecos << ":" << home << ":" << shell << "\n";
        }

        bool enumerate_and_match_uid(DWORD target_uid, std::ostream& out) {
            LPUSER_INFO_2 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0, resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetUserEnum(nullptr, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&p_buf,
                                     MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<USER_INFO_2> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        const auto& usr = safe_buf.get()[i];
                        if (get_rid_from_name(usr.usri2_name) == target_uid) {
                            print_user(out, usr);
                            return true;
                        }
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return false;
        }
    };

    // =========================================================================
    // Database: group (Local Groups)
    // Format: groupname:password:gid:user_list
    // =========================================================================
    class GroupHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "group"; }
        std::string description() const override { return "Security group database (Win32 Local Groups)"; }

        bool enumerate(std::ostream& out) override {
            LPLOCALGROUP_INFO_1 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetLocalGroupEnum(nullptr, 1, (LPBYTE*)&p_buf,
                                           MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        print_group(out, safe_buf.get()[i].lgrpi1_name);
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::wstring wkey = StringHelper::utf8_to_wide(key);
            LPLOCALGROUP_INFO_1 p_buf = nullptr;

            NET_API_STATUS status = NetLocalGroupGetInfo(nullptr, wkey.c_str(), 1, (LPBYTE*)&p_buf);
            NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

            if (status == NERR_Success && safe_buf) {
                print_group(out, safe_buf->lgrpi1_name);
                return true;
            }

            // Fallback: match by numeric GID (RID)
            try {
                DWORD target_rid = std::stoul(key);
                return enumerate_and_match_gid(target_rid, out);
            } catch (...) {
                return false;
            }
        }

    private:
        static DWORD get_group_rid(const std::wstring& group_name) {
            DWORD sid_size = 0, domain_size = 0;
            SID_NAME_USE use;
            LookupAccountNameW(nullptr, group_name.c_str(), nullptr, &sid_size, nullptr, &domain_size, &use);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> sid(sid_size);
                std::vector<wchar_t> domain(domain_size);
                if (LookupAccountNameW(nullptr, group_name.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use)) {
                    PUCHAR count = GetSidSubAuthorityCount(sid.data());
                    if (count && *count > 0) {
                        return *GetSidSubAuthority(sid.data(), *count - 1);
                    }
                }
            }
            return 544; // Default Administrators fallback
        }

        static void print_group(std::ostream& out, const wchar_t* wname) {
            if (!wname) return;
            std::string name = StringHelper::wide_to_utf8(wname);
            DWORD gid = get_group_rid(wname);

            // Fetch group members
            std::string members_str;
            LPLOCALGROUP_MEMBERS_INFO_3 p_mem = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;

            NET_API_STATUS status = NetLocalGroupGetMembers(nullptr, wname, 3, (LPBYTE*)&p_mem,
                                                            MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
            NetApiPtr<LOCALGROUP_MEMBERS_INFO_3> safe_mem(p_mem);

            if (status == NERR_Success && safe_mem) {
                for (DWORD j = 0; j < entries_read; ++j) {
                    if (j > 0) members_str += ",";
                    members_str += StringHelper::wide_to_utf8(safe_mem.get()[j].lgrmi3_domainandname);
                }
            }

            out << name << ":*:" << gid << ":" << members_str << "\n";
        }

        bool enumerate_and_match_gid(DWORD target_gid, std::ostream& out) {
            LPLOCALGROUP_INFO_1 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetLocalGroupEnum(nullptr, 1, (LPBYTE*)&p_buf,
                                           MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        const auto* name = safe_buf.get()[i].lgrpi1_name;
                        if (get_group_rid(name) == target_gid) {
                            print_group(out, name);
                            return true;
                        }
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return false;
        }
    };

