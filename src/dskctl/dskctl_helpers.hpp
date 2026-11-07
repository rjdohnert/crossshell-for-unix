#ifndef DSKCTL_HELPERS_HPP
#define DSKCTL_HELPERS_HPP

#include <windows.h>
#include <shlobj.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

std::string WideToUtf8(std::wstring_view value);
std::string PathToUtf8(const std::filesystem::path& path);
std::string FormatBytes(std::uint64_t bytes);
std::string FormatWin32Error(DWORD errorCode);
std::filesystem::path GetKnownFolderPath(REFKNOWNFOLDERID folderId,
                                         std::string& error);
std::filesystem::path GetWindowsDirectoryPath(std::string& error);
bool QueryPathAttributes(const std::filesystem::path& path, DWORD& attributes,
                         std::string& error);

#endif
