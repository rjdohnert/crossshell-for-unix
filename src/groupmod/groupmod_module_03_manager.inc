    // Windows Local Group Modification Manager
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result ModifyLocalGroup(
            const std::string& currentName,
            const std::optional<std::string>& newName,
            const std::optional<std::string>& comment,
            bool force
        ) {
            if (currentName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            if (!newName.has_value() && !comment.has_value()) {
                return { false, ERROR_INVALID_PARAMETER, "No modifications specified (use -n or -c)." };
            }

            std::wstring wCurrentName = StringConverter::ToWide(currentName);
            DWORD paramErr = 0;

            // Step 1: Modify Comment if specified
            if (comment.has_value()) {
                std::wstring wComment = StringConverter::ToWide(comment.value());
                LOCALGROUP_INFO_1002 lgi1002;
                lgi1002.lgrpi1002_comment = wComment.data();

                NET_API_STATUS status = NetLocalGroupSetInfo(
                    nullptr,
                    wCurrentName.c_str(),
                    1002,
                    reinterpret_cast<LPBYTE>(&lgi1002),
                    &paramErr
                );

                if (status != NERR_Success) {
                    return HandleApiError(status, force, "comment update");
                }
            }

            // Step 2: Rename group if specified
            if (newName.has_value()) {
                if (newName.value() == currentName) {
                    if (!force) {
                        return { true, 0, "Target name matches current name. No rename needed." };
                    }
                } else {
                    std::wstring wNewName = StringConverter::ToWide(newName.value());
                    LOCALGROUP_INFO_0 lgi0;
                    lgi0.lgrpi0_name = wNewName.data();

                    NET_API_STATUS status = NetLocalGroupSetInfo(
                        nullptr,
                        wCurrentName.c_str(),
                        0,
                        reinterpret_cast<LPBYTE>(&lgi0),
                        &paramErr
                    );

                    if (status != NERR_Success) {
                        return HandleApiError(status, force, "rename");
                    }
                }
            }

            return { true, 0, "Group modified successfully." };
        }

    private:
        static Result HandleApiError(DWORD status, bool force, std::string_view operation) {
            if ((status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) && force) {
                return { true, status, "Group does not exist (force ignored error)." };
            }
            if (status == NERR_GroupExists && force) {
                return { true, status, "New group name already exists (force ignored error)." };
            }
            return { false, status, ResolveErrorMessage(status) + " during " + std::string(operation) + "." };
        }

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
            case NERR_GroupExists:
                return "A group with the specified new name already exists.";
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

    // Command-line argument parser
