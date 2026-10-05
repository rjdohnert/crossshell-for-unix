    // =========================================================================
    // ACL & ACE Representation Models
    // =========================================================================
    enum class AcePrincipalType {
        User,
        Group,
        Mask,
        Other
    };

    struct ParsedAce {
        bool is_deny = false;
        bool is_inherited = false;
        AcePrincipalType principal_type = AcePrincipalType::User;
        std::string trustee_name;
        std::string trustee_sid;
        DWORD access_mask = 0;
        std::string rwx;
    };

    struct FileAclInfo {
        std::string path;
        std::string owner_name;
        std::string group_name;
        std::string owner_sid;
        std::string group_sid;
        std::vector<ParsedAce> access_entries;
        std::vector<ParsedAce> default_entries;
        bool has_extended_acl = false;
    };

