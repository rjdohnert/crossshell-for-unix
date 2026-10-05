class DiskCleanerEngine {
public:
    static bool MoveToRecycleBin(const fs::path& path) {
        std::wstring source = path.native();
        source.push_back(L'\0');
        source.push_back(L'\0');

        SHFILEOPSTRUCTW op = {};
        op.hwnd = nullptr;
        op.wFunc = FO_DELETE;
        op.pFrom = source.c_str();
        op.pTo = nullptr;
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        int result = SHFileOperationW(&op);
        return result == 0;
    }

    static void CollectPreviewItems(const fs::path& path, std::vector<std::string>& items, size_t& count) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            return;
        }

        if (!fs::is_directory(path, ec)) {
            items.push_back(path.string());
            ++count;
            return;
        }

        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            try {
                items.push_back(entry.path().string());
                ++count;

                std::error_code statusEc;
                if (fs::is_directory(entry.status(statusEc))) {
                    try {
                        CollectPreviewItems(entry.path(), items, count);
                    } catch (...) {
                    }
                }
            } catch (...) {
            }
        }
    }

    static bool CleanDirectory(const fs::path& path, const DskctlConfig& config, uint64_t& totalBytesFreed, size_t& filesRemoved, size_t& skippedEntries, size_t& errorCount) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            skippedEntries++;
            return true;
        }

        if (!SecurityInspector::IsAllowedPath(path)) {
            std::cerr << RED << "[SKIP] Refusing to operate on non-allowlisted path: " << path.string() << RESET << "\n";
            skippedEntries++;
            return false;
        }

        if (!fs::is_directory(path, ec)) {
            uint64_t size = fs::file_size(path, ec);
            if (ec) size = 0;

            if (config.verbose) {
                std::cout << "  " << (config.dryRun ? "[WOULD UNLINK] " : (config.trash ? "[TRASH] " : "[UNLINKING] ")) << path.string() << "\n";
            }

            if (config.dryRun) {
                totalBytesFreed += size;
                filesRemoved++;
                return true;
            }

            bool success = true;
            if (config.trash) {
                success = MoveToRecycleBin(path);
            } else {
                std::error_code delEc;
                success = fs::remove(path, delEc);
                if (!delEc && success) {
                    totalBytesFreed += size;
                    filesRemoved++;
                }
            }

            if (!success) {
                errorCount++;
                skippedEntries++;
                std::cerr << RED << "[SKIP] Locked or inaccessible: " << path.string() << RESET << "\n";
            }
            return success;
        }

        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            try {
                uint64_t entrySize = 0;
                if (fs::is_regular_file(entry.status())) {
                    entrySize = entry.file_size(ec);
                }

                if (config.verbose) {
                    std::cout << "  " << (config.dryRun ? "[WOULD UNLINK] " : (config.trash ? "[TRASH] " : "[UNLINKING] ")) << entry.path().string() << "\n";
                }

                if (!config.dryRun) {
                    std::error_code delEc;
                    bool success = true;
                    if (config.trash) {
                        success = MoveToRecycleBin(entry.path());
                    } else {
                        success = static_cast<bool>(fs::remove_all(entry.path(), delEc));
                    }
                    if (!delEc && success) {
                        totalBytesFreed += entrySize;
                        filesRemoved++;
                    } else {
                        errorCount++;
                        skippedEntries++;
                        std::cerr << RED << "[SKIP] Locked or inaccessible: " << entry.path().string() << RESET << "\n";
                    }
                } else {
                    totalBytesFreed += entrySize;
                    filesRemoved++;
                }
            } catch (...) {
                skippedEntries++;
                std::cerr << RED << "[SKIP] Could not process entry due to an exception: " << path.string() << RESET << "\n";
            }
        }

        return true;
    }

    static void CleanRecycleBin(const DskctlConfig& config, uint64_t& totalBytesFreed) {
        SHQUERYRBINFO rbInfo = { sizeof(SHQUERYRBINFO) };
        if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rbInfo))) {
            totalBytesFreed += rbInfo.i64Size;
            if (!config.dryRun) {
                SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            }
        }
    }
};
