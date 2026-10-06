#pragma once

#include "file_record.hpp"
#include "persistent_wal_journal.hpp"
#include "rpm.hpp"

class ArchiveExtractionEngine {
private:
    static fs::path sanitizePath(const fs::path& root, const std::string& raw);

public:
    static bool extract(const fs::path& rpmPath, std::streampos offset, const fs::path& root,
                        PersistentWalJournal& journal, std::vector<FileRecord>& outFiles,
                        const std::vector<FileRecord>& headerFiles);
};
