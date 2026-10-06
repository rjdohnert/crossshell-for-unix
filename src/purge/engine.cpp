#include "engine.hpp"

// SecureEraser
bool SecureEraser::EraseFile(const fs::path& filePath, uint64_t fileSize) {
    ScopedFileHandle hFile(CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
        NULL
    ));

    if (!hFile.IsValid()) {
        return false;
    }

    if (fileSize > 0) {
        std::vector<char> buffer(65536, 0);
        std::mt19937_64 rng(1337);
        std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFF);

        // Pass 1: Random Bitmask
        for (size_t i = 0; i < buffer.size(); i += 8) {
            uint64_t r = dist(rng);
            memcpy(&buffer[i], &r, sizeof(uint64_t));
        }

        uint64_t writtenTotal = 0;
        while (writtenTotal < fileSize) {
            DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), fileSize - writtenTotal));
            DWORD written = 0;
            if (!WriteFile(hFile.Get(), buffer.data(), toWrite, &written, NULL) || written == 0) break;
            writtenTotal += written;
        }

        // Pass 2: Cryptographic Zero Fill
        SetFilePointer(hFile.Get(), 0, NULL, FILE_BEGIN);
        std::fill(buffer.begin(), buffer.end(), 0x00);
        writtenTotal = 0;
        while (writtenTotal < fileSize) {
            DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), fileSize - writtenTotal));
            DWORD written = 0;
            if (!WriteFile(hFile.Get(), buffer.data(), toWrite, &written, NULL) || written == 0) break;
            writtenTotal += written;
        }
        FlushFileBuffers(hFile.Get());
    }

    return true;
}

// FileVersionExtractor
FileRecord FileVersionExtractor::Parse(const fs::directory_entry& entry) {
    FileRecord rec;
    rec.fullPath = entry.path();
    rec.fileSize = entry.file_size();
    rec.writeTime = entry.last_write_time();

    std::wstring filename = entry.path().filename().wstring();

    static const std::wregex vmsRegex(LR"((.+);(\d+)$)", std::regex::icase);
    static const std::wregex numExtRegex(LR"((.+)\.(\d+)$)", std::regex::icase);
    static const std::wregex vTagRegex(LR"((.*?)[._]v(\d+)(\.[^.]*)?$)", std::regex::icase);

    std::wsmatch match;
    if (std::regex_match(filename, match, vmsRegex)) {
        rec.baseKey = StringHelper::ToUpperW(match[1].str());
        rec.explicitVersion = std::stoll(match[2].str());
        rec.hasExplicitVersion = true;
    } else if (std::regex_match(filename, match, numExtRegex)) {
        rec.baseKey = StringHelper::ToUpperW(match[1].str());
        rec.explicitVersion = std::stoll(match[2].str());
        rec.hasExplicitVersion = true;
    } else if (std::regex_match(filename, match, vTagRegex)) {
        rec.baseKey = StringHelper::ToUpperW(match[1].str() + match[3].str());
        rec.explicitVersion = std::stoll(match[2].str());
        rec.hasExplicitVersion = true;
    } else {
        rec.baseKey = StringHelper::ToUpperW(filename);
        rec.explicitVersion = -1;
        rec.hasExplicitVersion = false;
    }

    return rec;
}

// PurgeEngine
int PurgeEngine::Execute(const PurgeOptions& opts, std::ostream& outStream) {
    fs::path searchDir = ".";
    std::wstring matchPattern = L"*";

    if (!opts.fileSpec.empty()) {
        fs::path p(opts.fileSpec);
        if (fs::is_directory(p)) {
            searchDir = p;
            matchPattern = L"*";
        } else {
            if (p.has_parent_path()) searchDir = p.parent_path();
            if (p.has_filename()) matchPattern = p.filename().wstring();
        }
    }

    if (!fs::exists(searchDir)) {
        outStream << "%RMS-E-DNF, directory not found " << searchDir.string() << "\n";
        return 2;
    }

    std::map<std::wstring, std::vector<FileRecord>> groups;

    try {
        for (const auto& entry : fs::directory_iterator(searchDir)) {
            if (!entry.is_regular_file()) continue;

            std::wstring filename = entry.path().filename().wstring();

            if (!StringHelper::WildcardMatch(matchPattern, filename)) continue;

            if (!opts.excludePattern.empty() && StringHelper::WildcardMatch(opts.excludePattern, filename)) {
                continue;
            }

            FileRecord rec = FileVersionExtractor::Parse(entry);
            groups[rec.baseKey].push_back(rec);
        }
    } catch (const std::exception& ex) {
        outStream << "%RMS-F-SYS, error accessing directory: " << ex.what() << "\n";
        return 4;
    }

    if (groups.empty()) {
        outStream << "%PURGE-W-NOFILES, no matching files found\n";
        return 0;
    }

    PurgeMetrics metrics;

    for (auto& [key, records] : groups) {
        if (records.size() <= static_cast<size_t>(opts.keepCount)) {
            continue;
        }

        std::sort(records.begin(), records.end(), [](const FileRecord& a, const FileRecord& b) {
            if (a.hasExplicitVersion && b.hasExplicitVersion) {
                return a.explicitVersion < b.explicitVersion;
            }
            return a.writeTime < b.writeTime;
        });

        size_t toDeleteCount = records.size() - static_cast<size_t>(opts.keepCount);

        for (size_t i = 0; i < toDeleteCount; ++i) {
            const auto& fileRec = records[i];
            bool proceed = true;

            if (opts.confirm) {
                std::cout << "DELETE " << fileRec.fullPath.string() << "? [N]: ";
                std::string response;
                if (!std::getline(std::cin, response) || response.empty()) {
                    proceed = false;
                } else {
                    char c = static_cast<char>(std::toupper(response[0]));
                    proceed = (c == 'Y' || c == 'T' || c == '1');
                }
            }

            if (!proceed) continue;

            DWORD attrs = GetFileAttributesW(fileRec.fullPath.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(fileRec.fullPath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }

            if (opts.erase) {
                SecureEraser::EraseFile(fileRec.fullPath, fileRec.fileSize);
            }

            std::error_code ec;
            if (fs::remove(fileRec.fullPath, ec)) {
                metrics.totalDeleted++;
                metrics.totalBytesFreed += fileRec.fileSize;

                if (opts.log) {
                    outStream << "%PURGE-I-FILPURG, " 
                              << fileRec.fullPath.string() 
                              << " deleted (" << StringHelper::FormatBytes(fileRec.fileSize) << ")\n";
                }
            } else {
                if (ec.value() == ERROR_ACCESS_DENIED) {
                    outStream << "%RMS-F-PRV, privilege violation deleting " << fileRec.fullPath.string() << "\n";
                } else if (ec.value() == ERROR_SHARING_VIOLATION || ec.value() == ERROR_LOCK_VIOLATION) {
                    outStream << "%RMS-E-FLK, file locked by another process " << fileRec.fullPath.string() << "\n";
                } else {
                    outStream << "%RMS-E-DEL, error deleting file " << fileRec.fullPath.string() << "\n";
                }
            }
        }
    }

    if (opts.log || opts.grandTotal || metrics.totalDeleted > 0) {
        outStream << "%PURGE-I-TOTAL, " << metrics.totalDeleted << " file" << (metrics.totalDeleted == 1 ? "" : "s")
                  << " deleted (" << StringHelper::FormatBytes(metrics.totalBytesFreed) << " freed)\n";
    }

    return 1;
}
