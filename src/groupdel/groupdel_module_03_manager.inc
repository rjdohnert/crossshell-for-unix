    // Windows Local Group Deletion Manager
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result DeleteLocalGroup(const std::string& groupName, bool force) {
            if (groupName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            std::wstring wGroupName = StringConverter::ToWide(groupName);

            NET_API_STATUS status = NetLocalGroupDel(
                nullptr,              // Local computer
                wGroupName.c_str()    // Group name to delete
            );

            if (status == NERR_Success) {
                return { true, 0, "Group deleted successfully." };
            }

            if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
                if (force) {
                    return { true, status, "Group does not exist (force ignored error)." };
                }
                return { false, status, "Group does not exist." };
            }

            return { false, status, ResolveErrorMessage(status) };
        }

    private:
        static std::string ResolveErrorMessage(DWORD errorCode) {
            switch (errorCode) {
            case ERROR_ACCESS_DENIED:
                return "Access denied. Administrator privileges (Elevated token) required.";
            case NERR_InvalidComputer:
                return "The computer name is invalid.";
            case NERR_NotPrimary:
                return "The operation is allowed only on the primary domain controller.";
            case NERR_GroupNotFound:
            case ERROR_NO_SUCH_ALIAS:
                return "The specified local group does not exist.";
            case ERROR_INVALID_NAME:
                return "The specified group name contains invalid characters.";
            case NERR_SpeGroupOp:
                return "Cannot perform this operation on built-in or special system groups.";
            default: {
                LPWSTR buffer = nullptr;
                size_t size = FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    reinterpret_cast<LPWSTR>(&buffer), 0, nullptr
                );
                std::string msg = size ? StringConverter::ToNarrow(buffer) : "Unknown Windows API error.";
                if (buffer) LocalFree(buffer);
                return StringConverter::Trim(msg);
            }
            }
        }
    };

