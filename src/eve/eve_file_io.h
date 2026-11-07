#pragma once

#include <filesystem>
#include <string>
#include <string_view>

enum class FileReadResult {
    Loaded,
    Missing,
    Error
};

FileReadResult ReadFileBytes(const std::filesystem::path& path, std::string& contents, std::string& error);
bool WriteFileAtomically(const std::filesystem::path& path, std::string_view contents, bool keep_backup, std::string& error);
