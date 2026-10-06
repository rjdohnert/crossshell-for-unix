#include "executable_path.hpp"

bool ResolveExecutablePath(const std::wstring& command, std::wstring& resolvedPath) {
    std::wstring cmd = command;
    size_t firstSpace = cmd.find_first_of(L" \t");
    std::wstring exe = (firstSpace == std::wstring::npos) ? cmd : cmd.substr(0, firstSpace);
    if (exe.empty()) return false;

    if ((exe.front() == L'"' && exe.back() == L'"') && exe.size() >= 2) {
        exe = exe.substr(1, exe.size() - 2);
    }

    wchar_t outPath[MAX_PATH] = {0};
    DWORD found = SearchPathW(NULL, exe.c_str(), L".exe", MAX_PATH, outPath, NULL);
    if (found > 0 && found < MAX_PATH) {
        resolvedPath = outPath;
        return true;
    }
    return false;
}
