#pragma once

#include "registryctl.hpp"
#include "undo_record.hpp"

class JournalEngine {
public:
    static void WriteUndoLog(const std::string& filename, const std::vector<UndoRecord>& records);

    static bool LoadUndoLog(const std::string& filename, std::vector<UndoRecord>& records, std::string& err);
};
