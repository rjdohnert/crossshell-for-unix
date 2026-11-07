#pragma once

#include <string>
#include <vector>

enum class EditorRunStatus {
    Completed,
    Interrupted,
    Failed
};

EditorRunStatus RunEditor(const std::vector<std::string>& files, bool read_only, std::string& error);
