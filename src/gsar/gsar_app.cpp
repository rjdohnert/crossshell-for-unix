#include "gsar_app.hpp"

#include "gsar_engine.hpp"
#include "gsar_file.hpp"

#include <fstream>
#include <iostream>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace {
bool SetBinaryStandardStreams() {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) return false;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return false;
#endif
    return true;
}

bool IsReparsePoint(const std::filesystem::path& path, bool& result,
                    std::string& error) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = "cannot inspect path attributes for " + path.string();
        return false;
    }
    result = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code statusError;
    const auto status = std::filesystem::symlink_status(path, statusError);
    if (statusError) {
        error = "cannot inspect path type for " + path.string() + ": " +
                statusError.message();
        return false;
    }
    result = std::filesystem::is_symlink(status);
#endif
    return true;
}

bool IsSameFile(const std::filesystem::path& left,
                const std::filesystem::path& right, bool& same,
                std::string& error) {
    std::error_code statusError;
    const bool rightExists = std::filesystem::exists(right, statusError);
    if (statusError) {
        error = "cannot inspect output path " + right.string() + ": " +
                statusError.message();
        return false;
    }
    if (!rightExists) {
        same = false;
        return true;
    }

    std::error_code leftError;
    const auto canonicalLeft = std::filesystem::weakly_canonical(left, leftError);
    if (leftError) {
        error = "cannot resolve input path: " + leftError.message();
        return false;
    }
#ifdef _WIN32
    std::error_code rightError;
    const auto canonicalRight = std::filesystem::weakly_canonical(right, rightError);
    if (rightError) {
        error = "cannot resolve output path: " + rightError.message();
        return false;
    }
    same = CompareStringOrdinal(canonicalLeft.c_str(), -1, canonicalRight.c_str(),
                                -1, TRUE) == CSTR_EQUAL;
#else
    std::error_code rightError;
    const auto canonicalRight = std::filesystem::weakly_canonical(right, rightError);
    if (rightError) {
        error = "cannot resolve output path: " + rightError.message();
        return false;
    }
    same = canonicalLeft == canonicalRight;
#endif
    return true;
}

void PrintFileSummary(const std::filesystem::path& path,
                      const SearchResult& result) {
    std::cerr << path.string() << ": " << result.matchCount
              << " occurrence(s) found.\n";
}
}

GSARApp::GSARApp(Options options) : m_options(std::move(options)) {}

bool GSARApp::processFile(const std::filesystem::path& inputPath,
                          const std::filesystem::path* outputPath) {
    std::error_code fileError;
    if (!std::filesystem::exists(inputPath, fileError) || fileError) {
        std::cerr << "[gsar] Error: Input file does not exist or cannot be accessed: "
                  << inputPath.string();
        if (fileError) std::cerr << ": " << fileError.message();
        std::cerr << '\n';
        return false;
    }
    if (!std::filesystem::is_regular_file(inputPath, fileError) || fileError) {
        std::cerr << "[gsar] Error: Input is not a regular file: "
                  << inputPath.string();
        if (fileError) std::cerr << ": " << fileError.message();
        std::cerr << '\n';
        return false;
    }

    bool inPlace = outputPath == nullptr && m_options.hasReplace &&
                   !m_options.countOnly;
    std::filesystem::path targetPath;
    if (outputPath != nullptr) {
        bool sameFile = false;
        std::string pathError;
        if (!IsSameFile(inputPath, *outputPath, sameFile, pathError)) {
            std::cerr << "[gsar] Error: " << pathError << '\n';
            return false;
        }
        inPlace = sameFile;
        targetPath = inPlace ? inputPath : *outputPath;
    } else if (inPlace) {
        targetPath = inputPath;
    }

    if (inPlace) {
        bool inputReparsePoint = false;
        std::string pathError;
        if (!IsReparsePoint(inputPath, inputReparsePoint, pathError)) {
            std::cerr << "[gsar] Error: " << pathError << '\n';
            return false;
        }
        if (inputReparsePoint) {
            std::cerr << "[gsar] Error: refusing in-place replacement of a "
                         "symbolic link or reparse point: "
                      << inputPath.string() << '\n';
            return false;
        }
        if (outputPath != nullptr) {
            bool outputReparsePoint = false;
            if (!IsReparsePoint(*outputPath, outputReparsePoint, pathError)) {
                std::cerr << "[gsar] Error: " << pathError << '\n';
                return false;
            }
            if (outputReparsePoint) {
                std::cerr << "[gsar] Error: refusing in-place replacement through a "
                             "symbolic link or reparse point: "
                          << outputPath->string() << '\n';
                return false;
            }
        }
    }

    std::error_code targetError;
    const bool targetExists = !targetPath.empty() &&
        std::filesystem::exists(targetPath, targetError);
    if (targetError) {
        std::cerr << "[gsar] Error: cannot inspect output path "
                  << targetPath.string() << ": " << targetError.message() << '\n';
        return false;
    }
    if (targetExists && !inPlace && !m_options.forceOverwrite) {
        std::cerr << "[gsar] Error: output already exists; use -f to replace it: "
                  << targetPath.string() << '\n';
        return false;
    }
    if (targetExists && !inPlace) {
        if (!std::filesystem::is_regular_file(targetPath, targetError) || targetError) {
            std::cerr << "[gsar] Error: output is not a regular file: "
                      << targetPath.string();
            if (targetError) std::cerr << ": " << targetError.message();
            std::cerr << '\n';
            return false;
        }
        bool reparsePoint = false;
        std::string pathError;
        if (!IsReparsePoint(targetPath, reparsePoint, pathError)) {
            std::cerr << "[gsar] Error: " << pathError << '\n';
            return false;
        }
        if (reparsePoint) {
            std::cerr << "[gsar] Error: refusing to replace a symbolic link or "
                         "reparse point: "
                      << targetPath.string() << '\n';
            return false;
        }
    }
    if (m_options.createBackup && !inPlace) {
        std::cerr << "[gsar] Error: backups are available only for in-place "
                     "replacement.\n";
        return false;
    }

    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "[gsar] Error: unable to open input file: "
                  << inputPath.string() << '\n';
        return false;
    }

    std::filesystem::perms permissions = std::filesystem::perms::unknown;
    if (targetExists) {
        const auto targetStatus = std::filesystem::status(targetPath, targetError);
        if (targetError) {
            std::cerr << "[gsar] Error: cannot inspect output permissions: "
                      << targetError.message() << '\n';
            return false;
        }
        permissions = targetStatus.permissions();
    }

    SearchEngine engine(m_options);
    SearchResult result;
    AtomicOutput output;
    if (!targetPath.empty()) {
        std::string outputError;
        if (!output.open(targetPath, inPlace || m_options.forceOverwrite,
                         permissions, outputError)) {
            std::cerr << "[gsar] Error: " << outputError << '\n';
            return false;
        }
        result = engine.processStream(input, &output.stream(), std::cerr);
    } else {
        result = engine.processStream(input, nullptr, std::cerr);
    }
    input.clear();
    input.close();
    if (input.fail()) {
        result.succeeded = false;
        result.error = "failed while closing input file";
    }
    if (!result.succeeded) {
        std::cerr << "[gsar] Error: " << result.error << " (" << inputPath.string()
                  << ")\n";
        return false;
    }

    if (inPlace && result.matchCount == 0) {
        if (m_options.verbose || m_options.countOnly) PrintFileSummary(inputPath, result);
        return true;
    }

    if (m_options.createBackup) {
        std::filesystem::path backupPath = inputPath;
        backupPath += m_options.backupExtension;
        std::error_code backupError;
        if (!std::filesystem::copy_file(inputPath, backupPath,
                                        std::filesystem::copy_options::none,
                                        backupError)) {
            std::cerr << "[gsar] Error: cannot create backup "
                      << backupPath.string() << ": " << backupError.message() << '\n';
            return false;
        }
    }

    if (!targetPath.empty()) {
        std::string outputError;
        if (!output.commit(outputError)) {
            std::cerr << "[gsar] Error: " << outputError << '\n';
            return false;
        }
    }

    if (m_options.listFilesOnly) {
        if (result.matchCount > 0) std::cout << inputPath.string() << '\n';
    } else if (m_options.countOnly || m_options.verbose ||
               (!m_options.hasReplace && result.matchCount > 0)) {
        PrintFileSummary(inputPath, result);
    }
    return true;
}

int GSARApp::runFilterMode() {
    if (!SetBinaryStandardStreams()) {
        std::cerr << "[gsar] Error: cannot enable binary standard streams.\n";
        return 1;
    }

    SearchEngine engine(m_options);
    SearchResult result = engine.processStream(
        std::cin, m_options.countOnly ? nullptr : &std::cout, std::cerr);
    if (!result.succeeded) {
        std::cerr << "[gsar] Error: " << result.error << '\n';
        return 1;
    }
    if (m_options.countOnly) {
        std::cerr << "Matches: " << result.matchCount << '\n';
    } else if (m_options.verbose) {
        std::cerr << "Matches: " << result.matchCount << " in "
                  << result.bytesRead << " byte(s).\n";
    }
    return 0;
}

int GSARApp::run() {
    if (m_options.filterPipeMode) return runFilterMode();

    bool succeeded = true;
    for (size_t i = 0; i < m_options.inputFiles.size(); ++i) {
        const std::filesystem::path* outputPath =
            m_options.outputFile.empty() ? nullptr : &m_options.outputFile;
        if (!processFile(m_options.inputFiles[i], outputPath)) succeeded = false;
    }
    return succeeded ? 0 : 1;
}
