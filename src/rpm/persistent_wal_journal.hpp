#pragma once

#include "rpm.hpp"

enum class WalAction : uint32_t { FileCreated = 1, FileReplaced = 2 };

class PersistentWalJournal {
private:
    fs::path journalDir;
    fs::path activeWalFile;
    fs::path stagingDir;
    std::ofstream walStream;
    std::vector<std::pair<WalAction, std::pair<fs::path, fs::path>>> loggedActions;

public:
    PersistentWalJournal(const fs::path& root = "");

    ~PersistentWalJournal();

    static void recoverInterruptedTransactions();

    bool safeWrite(const fs::path& dest, const std::vector<char>& data, uint32_t posixMode, std::string* outSha256 = nullptr);

    void rollback();

    void commit();
};
