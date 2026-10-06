#ifndef KNOWN_FOLDERS_HPP
#define KNOWN_FOLDERS_HPP

#include "search.hpp"

class KnownFolderLocator {
public:
    static std::optional<fs::path> GetKnownFolderPath(REFKNOWNFOLDERID folderId);
    static void PopulateUserCommonPaths(std::vector<fs::path>& targetPaths);
};

#endif // KNOWN_FOLDERS_HPP
