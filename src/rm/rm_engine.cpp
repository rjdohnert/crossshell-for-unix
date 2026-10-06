#include "console_prompter.hpp"
#include "pipe_stream_handler.hpp"
#include "rm_engine.hpp"
#include "rm_options.hpp"
#include "windows_file_operations.hpp"

RmEngine::RmEngine(RmOptions opts) : m_options(std::move(opts)) {}

int RmEngine::Run() {
        if (m_options.readStdin) {
            auto pipedPaths = PipeStreamHandler::ReadTargetsFromStream(m_options.nullDelimited);
            m_options.targets.insert(m_options.targets.end(), pipedPaths.begin(), pipedPaths.end());
        }

        if (m_options.targets.empty()) {
            if (!m_options.force) {
                std::cerr << "rm: missing operand\n";
                std::cerr << "Try 'rm --help' for more information.\n";
                return 1;
            }
            return 0;
        }

        if (m_options.interactive == InteractiveMode::Once) {
            if (m_options.targets.size() > 3 || m_options.recursive) {
                std::ostringstream msg;
                msg << "rm: remove " << m_options.targets.size() << " arguments";
                if (m_options.recursive) msg << " recursively";
                if (!ConsolePrompter::Prompt(msg.str())) {
                    return 0;
                }
            }
        }

        int exitCode = 0;
        for (const auto& target : m_options.targets) {
            if (!ProcessTarget(target)) {
                exitCode = 1;
            }
        }

        return exitCode;
    }

bool RmEngine::ProcessTarget(const fs::path& target) {
        std::error_code ec;
        fs::file_status status = fs::symlink_status(target, ec);

        if (!fs::exists(status)) {
            if (!m_options.force) {
                std::cerr << "rm: cannot remove '" << target.string() << "': No such file or directory\n";
                return false;
            }
            return true;
        }

        if (m_options.preserveRoot && WindowsFileOperations::IsRootPath(target)) {
            std::cerr << "rm: it is dangerous to operate recursively on '" << target.string() << "'\n";
            std::cerr << "rm: use --no-preserve-root to override this failsafe\n";
            return false;
        }

        if (fs::is_directory(status)) {
            return ProcessDirectory(target);
        } else {
            return ProcessFile(target);
        }
    }

bool RmEngine::ProcessFile(const fs::path& file) {
        if (m_options.interactive == InteractiveMode::Always) {
            std::string promptMsg = "rm: remove regular file '" + file.string() + "'";
            if (!ConsolePrompter::Prompt(promptMsg)) {
                return true;
            }
        } else if (!m_options.force && WindowsFileOperations::IsWriteProtected(file)) {
            std::string promptMsg = "rm: remove write-protected regular file '" + file.string() + "'";
            if (!ConsolePrompter::Prompt(promptMsg)) {
                return true;
            }
        }

        if (m_options.verbose) {
            std::cout << "removed '" << file.string() << "'\n";
        }

        if (m_options.dryRun) return true;

        if (m_options.force) {
            WindowsFileOperations::StripReadOnly(file);
        }

        std::error_code ec;
        if (!fs::remove(file, ec)) {
            if (!m_options.force) {
                std::cerr << "rm: cannot remove '" << file.string() << "': " << ec.message() << "\n";
                return false;
            }
        }

        m_removedCount++;
        return true;
    }

bool RmEngine::ProcessDirectory(const fs::path& dir) {
        if (!m_options.recursive && !m_options.removeEmptyDirs) {
            std::cerr << "rm: cannot remove '" << dir.string() << "': Is a directory\n";
            return false;
        }

        if (m_options.removeEmptyDirs && !m_options.recursive) {
            if (m_options.interactive == InteractiveMode::Always) {
                if (!ConsolePrompter::Prompt("rm: remove directory '" + dir.string() + "'")) {
                    return true;
                }
            }

            if (m_options.verbose) {
                std::cout << "removed directory '" << dir.string() << "'\n";
            }

            if (m_options.dryRun) return true;

            std::error_code ec;
            if (!fs::remove(dir, ec)) {
                std::cerr << "rm: failed to remove '" << dir.string() << "': Directory not empty\n";
                return false;
            }
            return true;
        }

        if (m_options.interactive == InteractiveMode::Always) {
            if (!ConsolePrompter::Prompt("rm: descend into directory '" + dir.string() + "'")) {
                return true;
            }
        }

        return RemoveDirectoryRecursive(dir);
    }

bool RmEngine::RemoveDirectoryRecursive(const fs::path& dir) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const auto& path = entry.path();
            if (entry.is_directory()) {
                RemoveDirectoryRecursive(path);
            } else {
                ProcessFile(path);
            }
        }

        if (m_options.interactive == InteractiveMode::Always) {
            if (!ConsolePrompter::Prompt("rm: remove directory '" + dir.string() + "'")) {
                return true;
            }
        }

        if (m_options.verbose) {
            std::cout << "removed directory '" << dir.string() << "'\n";
        }

        if (m_options.dryRun) return true;

        if (m_options.force) {
            WindowsFileOperations::StripReadOnly(dir);
        }

        fs::remove(dir, ec);
        return true;
    }
