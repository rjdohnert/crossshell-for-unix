    // Windows Local Group abstraction
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result CreateLocalGroup(const std::string& groupName, const std::string& comment, bool force) {
            if (groupName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            std::wstring wGroupName = StringConverter::ToWide(groupName);
            std::wstring wComment = StringConverter::ToWide(comment);

            LOCALGROUP_INFO_1 groupInfo;
            groupInfo.lgrpi1_name = wGroupName.data();
            groupInfo.lgrpi1_comment = wComment.empty() ? nullptr : wComment.data();

            DWORD paramErr = 0;
            NET_API_STATUS status = NetLocalGroupAdd(
                nullptr,            // Local computer
                1,                  // Level 1 (Name + Comment)
                reinterpret_cast<LPBYTE>(&groupInfo),
                &paramErr
            );

            if (status == NERR_Success) {
                return { true, 0, "Group created successfully." };
            }

            if (status == NERR_GroupExists) {
                if (force) {
                    return { true, NERR_GroupExists, "Group already exists (force ignored error)." };
                }
                return { false, NERR_GroupExists, "Group already exists." };
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
            case NERR_GroupExists:
                return "The local group already exists.";
            case ERROR_INVALID_NAME:
                return "The specified group name contains invalid characters.";
            case ERROR_NO_SUCH_MEMBER:
                return "One or more of the specified members do not exist.";
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

