// ============================================================================
// 5. COMMAND REDIRECTION & APPLICATION SHELL
// ============================================================================

class RedirectionParser {
public:
    static bool Parse(std::wstring& input, std::wstring& redirectFile, bool& append) {
        redirectFile.clear();
        append = false;

        bool in_quotes = false;
        size_t redirectPos = std::wstring::npos;

        for (size_t i = 0; i < input.length(); ++i) {
            wchar_t c = input[i];
            if (c == L'"') {
                in_quotes = !in_quotes;
            } else if (c == L'>' && !in_quotes) {
                redirectPos = i;
                break;
            }
        }

        if (redirectPos == std::wstring::npos) {
            return false;
        }

        if (redirectPos + 1 < input.length() && input[redirectPos + 1] == L'>') {
            append = true;
            std::wstring filePart = input.substr(redirectPos + 2);
            input = input.substr(0, redirectPos);
            std::vector<std::wstring> fileArgs = StringHelper::SplitArguments(filePart);
            if (!fileArgs.empty()) {
                redirectFile = fileArgs[0];
            }
        } else {
            append = false;
            std::wstring filePart = input.substr(redirectPos + 1);
            input = input.substr(0, redirectPos);
            std::vector<std::wstring> fileArgs = StringHelper::SplitArguments(filePart);
            if (!fileArgs.empty()) {
                redirectFile = fileArgs[0];
            }
        }

        return !redirectFile.empty();
    }
};

class FsctlApplication {
public:
    int Run() {
        ScopedConsoleMode::EnableVirtualTerminalProcessing();
        std::wstring current_path = PathHelper::GetCurrentDirectoryString();
        std::string input;
        std::vector<std::string> commandHistory;
        bool confirmDestructive = true;
        const bool isAdminUser = SystemInspector::IsCurrentUserAdministrator();
        SYSTEMTIME localTime;
        GetLocalTime(&localTime);

        std::cout << "\n\n";
        std::cout << "                       File System Control Shell v7.32\n\n";
        std::cout << "Copyright (C) 2026, Roberto J. Dohnert\n";
        std::cout << "All rights reserved.\n\n";
        std::cout << "Type 'help' to see the available commands.\n\n";
        std::cout << "Date: "
                  << std::setfill('0') << std::setw(2) << localTime.wMonth << "/"
                  << std::setw(2) << localTime.wDay << "/"
                  << std::setw(4) << localTime.wYear
                  << "  Time: "
                  << std::setw(2) << localTime.wHour << ":"
                  << std::setw(2) << localTime.wMinute << ":"
                  << std::setw(2) << localTime.wSecond
                  << "  Day: " << StringHelper::GetWeekdayName(localTime.wDayOfWeek) << "\n";
        std::cout << std::setfill(' ');

        while (true) {
            std::cout << "\n\033[1;36m[fsctl]";
            if (RemoteProtocolHelper::IsRemotePath(current_path)) {
                std::cout << "(remote)";
            }
            if (!confirmDestructive) {
                std::cout << "(confirm: off)";
            }
            std::cout << "\033[0m " << (isAdminUser ? "# " : ">> ");
            if (!std::getline(std::cin, input)) {
                break;
            }

            if (!input.empty()) {
                commandHistory.push_back(input);
                if (commandHistory.size() > 500) {
                    commandHistory.erase(commandHistory.begin());
                }
            }

            std::wstring winput = StringHelper::ToWide(input);
            std::wstring redirectFile;
            bool append = false;
            ScopedOutputRedirect redirectGuard;

            if (RedirectionParser::Parse(winput, redirectFile, append)) {
                std::wstring redirectPath = PathHelper::NormalizePath(current_path, redirectFile);
                if (!PathHelper::EnsureParentDirectoryExists(redirectPath)) {
                    std::cout << "\033[1;31mError: Unable to create parent directory for redirect target.\033[0m\n";
                    continue;
                }
                redirectGuard.Redirect(redirectPath, append);
            }

            std::vector<std::wstring> wargs = StringHelper::SplitArguments(winput);
            if (wargs.empty()) {
                continue;
            }

            std::string command = StringHelper::ToNarrow(wargs[0]);

            if (command == "exit" || command == "quit") {
                break;
            }
            else if (command == "drive") {
                SystemInspector::ListDrives();
            }
            else if (command == "shares" || command == "net") {
                SystemInspector::ListNetworkShares();
            }
            else if (command == "ls" || command == "dir") {
                bool isTree = false;
                bool showHidden = false;
                std::wstring targetPath = current_path;
                bool targetSpecified = false;
                bool invalidOption = false;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-t" || wargs[i] == L"--tree") {
                        isTree = true;
                    } else if (wargs[i] == L"-h" || wargs[i] == L"--hidden" || wargs[i] == L"-a" || wargs[i] == L"--all") {
                        showHidden = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown " << command << " option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: " << command << " [-h|--hidden] [-t|--tree] [path]\n";
                        invalidOption = true;
                        break;
                    } else {
                        if (!targetSpecified) {
                            targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[i]);
                            targetSpecified = true;
                        }
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (isTree) {
                    if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                        TreeRenderer::RunTree(targetPath);
                    } else {
                        std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(targetPath) << "\033[0m\n";
                    }
                } else {
                    if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                        FileOperator::ListDirectory(targetPath, showHidden);
                    } else {
                        std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(targetPath) << "\033[0m\n";
                    }
                }
            }
            else if (command == "tree") {
                std::wstring targetPath = current_path;
                if (wargs.size() > 1) {
                    targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                }
                if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                    TreeRenderer::RunTree(targetPath);
                } else {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs.size() > 1 ? wargs[1] : L".") << "\033[0m\n";
                }
            }
            else if (command == "cd") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory path required.\033[0m\n";
                    continue;
                }

                std::wstring target_path = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);

                if (RemoteProtocolHelper::IsRemotePath(target_path)) {
                    current_path = target_path;
                } else if (PathHelper::PathExists(target_path) && PathHelper::IsDirectoryPath(target_path)) {
                    current_path = target_path;
                    SetCurrentDirectoryW(current_path.c_str());
                } else {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                }
            }
            else if (command == "pwd") {
                std::cout << StringHelper::ToNarrow(current_path) << "\n";
            }
            else if (command == "mkdir") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory name required.\033[0m\n";
                    continue;
                }

                std::wstring newDir = PathHelper::NormalizePath(current_path, wargs[1]);
                if (PathHelper::PathExists(newDir)) {
                    std::cout << "\033[1;31mError: Path already exists: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (!PathHelper::EnsureParentDirectoryExists(newDir)) {
                    std::cout << "\033[1;31mError: Unable to create parent directories for: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (CreateDirectoryW(newDir.c_str(), NULL) != 0) {
                    std::cout << "\033[1;32mDirectory created successfully: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_ALREADY_EXISTS) {
                        std::cout << "\033[1;31mError: Directory already exists or creation failed.\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError creating directory (code " << err << ").\033[0m\n";
                    }
                }
            }
            else if (command == "rmdir") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory name required.\033[0m\n";
                    continue;
                }

                std::wstring targetDir = PathHelper::NormalizePath(current_path, wargs[1]);
                if (!PathHelper::PathExists(targetDir)) {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (!PathHelper::IsDirectoryPath(targetDir)) {
                    std::cout << "\033[1;31mError: Target is not a directory: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"deletion of directory '" + targetDir + L"'")) {
                    std::cout << "\033[90mCancelled.\033[0m\n";
                } else if (RemoveDirectoryW(targetDir.c_str()) != 0) {
                    std::cout << "\033[1;32mDirectory removed successfully: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else {
                    std::cout << "\033[1;31mError removing directory (code " << GetLastError() << ").\033[0m\n";
                }
            }
            else if (command == "trash") {
                bool force = false;
                bool invalidOption = false;
                std::vector<std::wstring> targets;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-f" || wargs[i] == L"--force") {
                        force = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown trash option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: trash [-f|--force] <path> [path2 ...]\n";
                        invalidOption = true;
                        break;
                    } else {
                        targets.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (targets.empty()) {
                    std::cout << "Usage: trash [-f|--force] <path> [path2 ...]\n";
                    continue;
                }

                for (const auto& target : targets) {
                    std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, target);
                    if (RemoteProtocolHelper::IsRemotePath(targetPath)) {
                        std::cout << "\033[1;31mError: trash is supported for local paths only.\033[0m\n";
                        continue;
                    }
                    if (!PathHelper::PathExists(targetPath)) {
                        if (force) {
                            std::cout << "\033[90mSkipped (not found): " << StringHelper::ToNarrow(target) << "\033[0m\n";
                        } else {
                            std::cout << "\033[1;31mError: Path does not exist: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                        }
                        continue;
                    }

                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"move to recycle bin '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    if (FileOperator::MovePathToRecycleBin(targetPath)) {
                        std::cout << "\033[1;32mMoved to recycle bin: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError moving to recycle bin: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                    }
                }
            }
            else if (command == "rm") {
                bool recursive = false;
                bool force = false;
                bool invalidOption = false;
                std::vector<std::wstring> positional;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-r" || wargs[i] == L"--recursive") {
                        recursive = true;
                    } else if (wargs[i] == L"-f" || wargs[i] == L"--force") {
                        force = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown rm option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                        invalidOption = true;
                        break;
                    } else {
                        positional.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (positional.size() != 1) {
                    std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                    continue;
                }

                std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, positional[0]);
                if (RemoteProtocolHelper::IsRemotePath(targetPath)) {
                    std::cout << "\033[1;31mError: rm is supported for local paths only.\033[0m\n";
                    continue;
                }

                if (!PathHelper::PathExists(targetPath)) {
                    if (force) {
                        std::cout << "\033[90mSkipped (not found): " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError: Path does not exist: " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    }
                    continue;
                }

                if (PathHelper::IsDirectoryPath(targetPath)) {
                    if (!recursive) {
                        std::cout << "\033[1;31mError: Target is a directory. Use rm -r or rmdir.\033[0m\n";
                        std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                        continue;
                    }

                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"recursive delete of directory '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    uintmax_t removedCount = 0;
                    if (FileOperator::RemoveDirectoryTree(targetPath, removedCount)) {
                        std::cout << "\033[1;32mRemoved directory tree: " << StringHelper::ToNarrow(positional[0])
                                  << " (" << removedCount << " item(s))\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError removing directory tree.\033[0m\n";
                    }
                } else {
                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"delete file '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    if (DeleteFileW(targetPath.c_str()) != 0) {
                        std::cout << "\033[1;32mFile removed successfully: " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError removing file (code " << GetLastError() << ").\033[0m\n";
                    }
                }
            }
            else if (command == "touch") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: At least one file path is required.\033[0m\n";
                    std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                    continue;
                }

                bool noCreate = false;
                bool invalidOption = false;
                std::vector<std::wstring> touchTargets;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-c" || wargs[i] == L"--no-create") {
                        noCreate = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown touch option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                        invalidOption = true;
                        break;
                    } else {
                        touchTargets.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (touchTargets.empty()) {
                    std::cout << "\033[1;31mError: At least one file path is required.\033[0m\n";
                    std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                    continue;
                }

                for (const auto& target : touchTargets) {
                    std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, target);
                    FileOperator::TouchPath(targetPath, noCreate);
                }
            }
            else if (command == "copy") {
                if (wargs.size() < 3) {
                    std::cout << "Usage: copy <source_file> <destination_file>\n";
                    continue;
                }

                std::wstring sourcePath = PathHelper::NormalizePath(current_path, wargs[1]);
                std::wstring destinationPath = PathHelper::NormalizePath(current_path, wargs[2]);

                if (PathHelper::PathExists(destinationPath) && PathHelper::IsDirectoryPath(destinationPath)) {
                    std::cout << "\033[1;31mError: Destination path is a directory.\033[0m\n";
                } else if (!PathHelper::PathExists(sourcePath)) {
                    std::cout << "\033[1;31mError: Source file does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (PathHelper::IsDirectoryPath(sourcePath)) {
                    std::cout << "\033[1;31mError: Source path is a directory. copy supports files only.\033[0m\n";
                } else if (CopyFileW(sourcePath.c_str(), destinationPath.c_str(), TRUE) != 0) {
                    std::cout << "\033[1;32mFile copied successfully.\033[0m\n";
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {
                        std::cout << "\033[1;31mError: Destination file already exists.\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError copying file (code " << err << ").\033[0m\n";
                    }
                }
            }
            else if (command == "move") {
                if (wargs.size() < 3) {
                    std::cout << "Usage: move <source_file> <destination_file>\n";
                    continue;
                }

                std::wstring sourcePath = PathHelper::NormalizePath(current_path, wargs[1]);
                std::wstring destinationPath = PathHelper::NormalizePath(current_path, wargs[2]);

                if (PathHelper::PathExists(destinationPath) && PathHelper::IsDirectoryPath(destinationPath)) {
                    std::cout << "\033[1;31mError: Destination path is a directory.\033[0m\n";
                } else if (!PathHelper::PathExists(sourcePath)) {
                    std::cout << "\033[1;31mError: Source file does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (PathHelper::IsDirectoryPath(sourcePath)) {
                    std::cout << "\033[1;31mError: Source path is a directory. move supports files only.\033[0m\n";
                } else if (MoveFileW(sourcePath.c_str(), destinationPath.c_str()) != 0) {
                    std::cout << "\033[1;32mFile moved successfully.\033[0m\n";
                } else {
                    std::cout << "\033[1;31mError moving file (code " << GetLastError() << ").\033[0m\n";
                }
            }
            else if (command == "cat" || command == "type") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: File path required.\033[0m\n";
                    continue;
                }
                std::wstring targetFile = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                FileOperator::CatFile(targetFile);
            }
            else if (command == "stat") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Path required.\033[0m\n";
                    continue;
                }
                std::wstring targetPath = PathHelper::NormalizePath(current_path, wargs[1]);
                FileOperator::StatPath(targetPath);
            }
            else if (command == "find") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Search pattern required.\033[0m\n";
                    continue;
                }
                FileOperator::RunFind(current_path, wargs[1]);
            }
            else if (command == "diff") {
                if (wargs.size() != 3) {
                    std::cout << "Usage: diff <file1> <file2>\n";
                    continue;
                }

                std::wstring leftPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                std::wstring rightPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[2]);
                FileOperator::ShowFileDiff(leftPath, rightPath);
            }
            else if (command == "history") {
                if (wargs.size() > 1 && wargs[1] == L"clear") {
                    commandHistory.clear();
                    std::cout << "\033[1;32mHistory cleared.\033[0m\n";
                } else {
                    if (commandHistory.empty()) {
                        std::cout << "<History is empty>\n";
                    } else {
                        for (size_t i = 0; i < commandHistory.size(); ++i) {
                            std::cout << std::setw(4) << (i + 1) << "  " << commandHistory[i] << "\n";
                        }
                    }
                }
            }
            else if (command == "confirm") {
                if (wargs.size() == 1 || (wargs.size() > 1 && wargs[1] == L"status")) {
                    std::cout << "Destructive confirmations are " << (confirmDestructive ? "ON" : "OFF") << ".\n";
                } else if (wargs[1] == L"on") {
                    confirmDestructive = true;
                    std::cout << "\033[1;32mDestructive confirmations enabled.\033[0m\n";
                } else if (wargs[1] == L"off") {
                    confirmDestructive = false;
                    std::cout << "\033[1;33mDestructive confirmations disabled.\033[0m\n";
                } else {
                    std::cout << "Usage: confirm [on|off|status]\n";
                }
            }
            else if (command == "help") {
                std::cout << "\n";
                std::cout << "\033[1;36mAvailable Commands:\033[0m\n"
                          << "\n"
                          << "  drive             - List all available drives with storage info\n"
                          << "  shares            - List available network shares\n"
                          << "  ls / dir [-h] [path] [-t] - List files/folders (hidden files hidden by default, use -h/--hidden; use -t/--tree for tree layout)\n"
                          << "  tree [path]       - List directory structure recursively in tree format for a path\n"
                          << "  cd <directory>    - Move to a different directory or remote URL (ftp:// or ssh://)\n"
                          << "  pwd               - Print current local directory or remote URI\n"
                          << "  mkdir <name>      - Create a new folder, including parent directories when needed (supports spaces/quotes)\n"
                          << "  rmdir <name>      - Remove an empty folder (confirmation controlled by 'confirm')\n"
                          << "  trash [-f] <path...> - Move file/folder(s) to recycle bin (-f skips confirmation)\n"
                          << "  rm [-r] [-f] <path> - Remove a file or recursively remove a directory with -r (-f skips confirmation)\n"
                          << "  touch [-c] <file...> - Create/update file timestamps (-c skips creation)\n"
                          << "  copy <src> <dst>  - Copy a file to a new path (supports spaces/quotes)\n"
                          << "  move <src> <dst>  - Move/rename a file to a new path (supports spaces/quotes)\n"
                          << "  cat / type <file> - Display text file content from local or remote paths\n"
                          << "  stat <path>       - Display detailed properties and metadata of a path\n"
                          << "  find <pattern>    - Recursively search for matching file/folder names\n"
                          << "  diff <f1> <f2>    - Compare two local text files line by line\n"
                          << "  history [clear]   - Show recent command history or clear it\n"
                          << "  confirm [on|off|status] - Toggle confirmations for destructive actions\n"
                          << "  exit / quit       - Terminate the program\n\n"
                          << "\033[1;36mRemote Protocols:\033[0m\n"
                          << "  ftp://host/path     - Browse directories and read files via FTP\n"
                          << "  ssh://user@host/path - Browse directories and read files via SSH when ssh.exe or plink.exe is installed\n"
                          << "  Examples:\n"
                          << "    cd ftp://ftp.example.com/pub\n"
                          << "    ls ftp://ftp.example.com/pub\n"
                          << "    cat ssh://user@example.com/home/user/file.txt\n\n"
                          << "\033[1;36mOutput Redirection:\033[0m\n"
                          << "  You can redirect command output to a file using '>' (overwrite) or '>>' (append).\n"
                          << "  Examples:\n"
                          << "    ls -t > tree_layout.txt\n"
                          << "    drive >> space_history.txt\n"
                          << "    cat file.txt > copy.txt\n";
            }
            else if (!command.empty()) {
                std::cout << "\033[1;31mUnknown command: '" << command << "'. Type 'help' for options.\033[0m\n";
            }
        }

        return 0;
    }
};

