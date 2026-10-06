#include "file_renamer.hpp"
#include "regex_strategy.hpp"
#include "rename_options.hpp"
#include "substring_strategy.hpp"

[[nodiscard]] bool FileRenamer::askUserConfirmation(const fs::path& target) const {
        std::cout << "rename: overwrite '" << target.string() << "'? (y/n): ";
        std::string response;
        if (std::getline(std::cin, response)) {
            return !response.empty() && (response[0] == 'y' || response[0] == 'Y');
        }
        return false;
    }

bool FileRenamer::executeMove(const fs::path& source, const fs::path& dest) {
#ifdef _WIN32
        // Win32 MoveFileExW provides atomic replacement and handles casing changes
        DWORD flags = MOVEFILE_COPY_ALLOWED;
        if (!options_.noOverwrite) {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }

        if (!MoveFileExW(source.c_str(), dest.c_str(), flags)) {
            DWORD error = GetLastError();
            std::cerr << "rename: cannot rename '" << source.string()
                      << "' to '" << dest.string() << "': Error " << error << "\n";
            return false;
        }
        return true;
#else
        std::error_code ec;
        fs::rename(source, dest, ec);
        if (ec) {
            std::cerr << "rename: cannot rename '" << source.string()
                      << "' to '" << dest.string() << "': " << ec.message() << "\n";
            return false;
        }
        return true;
#endif
    }

FileRenamer::FileRenamer(RenameOptions options) : options_(std::move(options)) {
        if (options_.useRegex) {
            strategy_ = std::make_unique<RegexStrategy>(
                options_.searchPattern,
                options_.replacement,
                options_.replaceAll,
                options_.ignoreCase
            );
        } else {
            strategy_ = std::make_unique<SubstringStrategy>(
                options_.searchPattern,
                options_.replacement,
                options_.replaceAll,
                options_.replaceLast,
                options_.ignoreCase
            );
        }
    }

int FileRenamer::execute() {
        for (const auto& filePath : options_.targetFiles) {
            ++processedCount_;

            std::error_code ec;
            if (!fs::exists(filePath, ec)) {
                std::cerr << "rename: cannot access '" << filePath.string()
                          << "': No such file or directory\n";
                ++errorCount_;
                continue;
            }

            std::string oldFilename = filePath.filename().string();
            std::string newFilename;

            try {
                newFilename = strategy_->transform(oldFilename);
            } catch (const std::regex_error& e) {
                std::cerr << "rename: regular expression error: " << e.what() << "\n";
                return 2;
            }

            // If replacement produced no change, skip
            if (oldFilename == newFilename) {
                continue;
            }

            fs::path destination = filePath.parent_path() / newFilename;

            // Handle destination collisions (excluding same-file case changes on Windows)
            bool sameFile = false;
            if (fs::exists(destination, ec)) {
                std::error_code eqEc;
                sameFile = fs::equivalent(filePath, destination, eqEc);

                if (!sameFile) {
                    if (options_.noOverwrite) {
                        if (options_.verbose) {
                            std::cout << "skipping '" << filePath.string()
                                      << "' (target exists)\n";
                        }
                        ++skippedCount_;
                        continue;
                    }

                    if (options_.interactive && !askUserConfirmation(destination)) {
                        if (options_.verbose) {
                            std::cout << "skipping '" << filePath.string() << "'\n";
                        }
                        ++skippedCount_;
                        continue;
                    }
                }
            }

            if (options_.verbose || options_.dryRun) {
                std::cout << "'" << filePath.string() << "' -> '"
                          << destination.string() << "'\n";
            }

            if (options_.dryRun) {
                ++renamedCount_;
                continue;
            }

            if (executeMove(filePath, destination)) {
                ++renamedCount_;
            } else {
                ++errorCount_;
            }
        }

        return (errorCount_ > 0) ? 1 : 0;
    }
