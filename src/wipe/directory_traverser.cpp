#include "directory_traverser.hpp"
#include "file_wiper.hpp"
#include "pass_config.hpp"
#include "path_utils.hpp"

bool DirectoryTraverser::WipeDirectory(const std::string& dirpath, const std::vector<PassConfig>& passes,
                              bool recursive, bool force, bool verbose, bool interactive) {
        std::string search_path = PathUtils::JoinPath(dirpath, "*");
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);

        if (hFind == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Cannot access directory: " << dirpath << "\n";
            return false;
        }

        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;

            std::string full_path = PathUtils::JoinPath(dirpath, name);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    if (force) SetFileAttributesA(full_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                    RemoveDirectoryA(full_path.c_str());
                } else if (recursive) {
                    WipeDirectory(full_path, passes, recursive, force, verbose, interactive);
                }
            } else {
                if (interactive) {
                    std::cout << "Wipe file '" << full_path << "'? (y/N): ";
                    char ans = 'n';
                    std::cin >> ans;
                    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                    if (ans != 'y' && ans != 'Y') continue;
                }
                FileWiper::WipeFile(full_path, passes, force, verbose);
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);

        if (force) SetFileAttributesA(dirpath.c_str(), FILE_ATTRIBUTE_NORMAL);

        std::string current_dir = dirpath;
        for (int i = 0; i < 3; ++i) {
            std::string new_dir = PathUtils::GetRandomFilePath(current_dir);
            if (MoveFileA(current_dir.c_str(), new_dir.c_str())) {
                current_dir = new_dir;
            } else {
                break;
            }
        }

        if (RemoveDirectoryA(current_dir.c_str())) {
            if (verbose) {
                std::cout << "[+] Removed directory: " << dirpath << "\n";
            }
            return true;
        } else {
            std::cerr << "[-] Failed to remove directory: " << current_dir << " (Error " << GetLastError() << ")\n";
            return false;
        }
    }
