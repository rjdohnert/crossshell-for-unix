#include "known_folders.hpp"

std::optional<fs::path> KnownFolderLocator::GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, NULL, &path);
    if (SUCCEEDED(hr) && path != nullptr) {
        fs::path result(path);
        CoTaskMemFree(path);
        return result;
    }
    return std::nullopt;
}

void KnownFolderLocator::PopulateUserCommonPaths(std::vector<fs::path>& targetPaths) {
    auto downloads = GetKnownFolderPath(FOLDERID_Downloads);
    auto docs      = GetKnownFolderPath(FOLDERID_Documents);
    auto desktop   = GetKnownFolderPath(FOLDERID_Desktop);
    auto pictures  = GetKnownFolderPath(FOLDERID_Pictures);
    auto music     = GetKnownFolderPath(FOLDERID_Music);
    auto videos    = GetKnownFolderPath(FOLDERID_Videos);

    std::error_code ec;
    auto addIfExists = [&](const std::optional<fs::path>& p) {
        if (p && fs::exists(*p, ec)) {
            if (std::find(targetPaths.begin(), targetPaths.end(), *p) == targetPaths.end()) {
                targetPaths.push_back(*p);
            }
        }
    };

    addIfExists(downloads);
    addIfExists(docs);
    addIfExists(desktop);
    addIfExists(pictures);
    addIfExists(music);
    addIfExists(videos);
}
