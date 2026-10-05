    // =========================================================================
    // Security Inspector Layer (Win32 DACL Extractor)
    // =========================================================================
    class SecurityInspector {
    public:
        static bool inspect_file(const fs::path& file_path, const Options& opts, FileAclInfo& out_info) {
            std::wstring wpath = file_path.wstring();

            PSECURITY_DESCRIPTOR pSD = nullptr;
            PSID pOwner = nullptr;
            PSID pGroup = nullptr;
            PACL pDacl = nullptr;

            DWORD status = GetNamedSecurityInfoW(
                wpath.c_str(),
                SE_FILE_OBJECT,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                &pOwner,
                &pGroup,
                &pDacl,
                nullptr,
                &pSD
            );

            if (status != ERROR_SUCCESS || !pSD) {
                return false;
            }

            // RAII wrapper for LocalFree on Security Descriptor
            std::unique_ptr<void, decltype(&LocalFree)> sd_guard(pSD, LocalFree);

            out_info.path = file_path.string();
            out_info.owner_name = Win32Helper::resolve_sid_to_name(pOwner, opts.numeric_ids);
            out_info.owner_sid = Win32Helper::sid_to_string(pOwner);
            out_info.group_name = Win32Helper::resolve_sid_to_name(pGroup, opts.numeric_ids);
            out_info.group_sid = Win32Helper::sid_to_string(pGroup);

            if (pDacl) {
                ACL_SIZE_INFORMATION acl_info{};
                if (GetAclInformation(pDacl, &acl_info, sizeof(acl_info), AclSizeInformation)) {
                    for (DWORD i = 0; i < acl_info.AceCount; ++i) {
                        LPVOID pAce = nullptr;
                        if (!GetAce(pDacl, i, &pAce)) continue;

                        PACE_HEADER pHeader = reinterpret_cast<PACE_HEADER>(pAce);
                        ParsedAce ace_entry;

                        ace_entry.is_inherited = (pHeader->AceFlags & INHERITED_ACE) != 0;
                        const bool is_container_inherit = (pHeader->AceFlags & (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)) != 0;

                        PSID sid = nullptr;
                        if (pHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                            auto* allowed = reinterpret_cast<ACCESS_ALLOWED_ACE*>(pAce);
                            ace_entry.is_deny = false;
                            ace_entry.access_mask = allowed->Mask;
                            sid = reinterpret_cast<PSID>(&allowed->SidStart);
                        } else if (pHeader->AceType == ACCESS_DENIED_ACE_TYPE) {
                            auto* denied = reinterpret_cast<ACCESS_DENIED_ACE*>(pAce);
                            ace_entry.is_deny = true;
                            ace_entry.access_mask = denied->Mask;
                            sid = reinterpret_cast<PSID>(&denied->SidStart);
                        } else {
                            continue; // Skip audit/unsupported ACE types
                        }

                        SID_NAME_USE sid_type = SidTypeUnknown;
                        ace_entry.trustee_name = Win32Helper::resolve_sid_to_name(sid, opts.numeric_ids, &sid_type);
                        ace_entry.trustee_sid = Win32Helper::sid_to_string(sid);

                        if (sid_type == SidTypeGroup || sid_type == SidTypeWellKnownGroup || sid_type == SidTypeAlias) {
                            ace_entry.principal_type = AcePrincipalType::Group;
                        } else {
                            ace_entry.principal_type = AcePrincipalType::User;
                        }

                        ace_entry.rwx = Win32Helper::mask_to_rwx(ace_entry.access_mask);

                        if (is_container_inherit) {
                            out_info.default_entries.push_back(ace_entry);
                        }
                        out_info.access_entries.push_back(ace_entry);
                    }
                }
            }

            out_info.has_extended_acl = (out_info.access_entries.size() > 3);
            return true;
        }
    };

