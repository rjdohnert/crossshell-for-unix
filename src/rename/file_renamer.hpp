#pragma once

#include "rename_options.hpp"
#include "rename_strategy.hpp"
#include "rename.hpp"

class FileRenamer {
private:
    RenameOptions options_;
    std::unique_ptr<IRenameStrategy> strategy_;
    size_t processedCount_ = 0;
    size_t renamedCount_   = 0;
    size_t errorCount_     = 0;
    size_t skippedCount_   = 0;

    [[nodiscard]] bool askUserConfirmation(const fs::path& target) const;

    bool executeMove(const fs::path& source, const fs::path& dest);

public:
    explicit FileRenamer(RenameOptions options);

    int execute();
};
