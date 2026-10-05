    // =========================================================================
    // ACL Output Formatter
    // =========================================================================
    class AclFormatter {
    public:
        static void format(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            if (opts.skip_base_only && !info.has_extended_acl) {
                return;
            }

            if (opts.tabular) {
                format_tabular(out, info, opts);
            } else {
                format_posix(out, info, opts);
            }
        }

    private:
        static void format_posix(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            if (!opts.omit_header) {
                out << "# file: " << info.path << "\n";
                out << "# owner: " << info.owner_name << "\n";
                out << "# group: " << info.group_name << "\n";
            }

            if (opts.show_access) {
                for (const auto& ace : info.access_entries) {
                    if (ace.is_deny) {
                        out << "deny:";
                    }
                    if (ace.principal_type == AcePrincipalType::Group) {
                        out << "group:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    } else {
                        out << "user:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    }
                }
            }

            if (opts.show_default && !info.default_entries.empty()) {
                for (const auto& ace : info.default_entries) {
                    out << "default:";
                    if (ace.is_deny) out << "deny:";
                    if (ace.principal_type == AcePrincipalType::Group) {
                        out << "group:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    } else {
                        out << "user:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    }
                }
            }

            out << "\n";
            out.flush();
        }

        static void format_tabular(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            out << std::left << std::setw(30) << info.path
                << std::setw(25) << info.owner_name
                << std::setw(25) << info.group_name << "\n";

            for (const auto& ace : info.access_entries) {
                out << "  " << (ace.is_deny ? "DENY  " : "ALLOW ")
                    << std::setw(8) << (ace.principal_type == AcePrincipalType::Group ? "group" : "user")
                    << std::setw(30) << ace.trustee_name
                    << std::setw(6) << ace.rwx
                    << (ace.is_inherited ? "[inherited]" : "[explicit]") << "\n";
            }
            out << "\n";
            out.flush();
        }
    };

