#include "file_wiper.hpp"
#include "pass_config.hpp"
#include "pass_generator.hpp"
#include "path_utils.hpp"

bool FileWiper::SecureRenameAndDelete(const std::string& filepath, bool verbose) {
        std::string current_path = filepath;
        
        for (int i = 0; i < 3; ++i) {
            std::string new_path = PathUtils::GetRandomFilePath(current_path);
            if (MoveFileA(current_path.c_str(), new_path.c_str())) {
                current_path = new_path;
            } else {
                break;
            }
        }

        if (DeleteFileA(current_path.c_str())) {
            if (verbose) {
                std::cout << "[+] Wiped & Deleted: " << filepath << "\n";
            }
            return true;
        } else {
            std::cerr << "[-] Failed to delete: " << current_path << " (Error " << GetLastError() << ")\n";
            return false;
        }
    }

bool FileWiper::WipeFile(const std::string& filepath, const std::vector<PassConfig>& passes, bool force, bool verbose) {
        if (force) {
            SetFileAttributesA(filepath.c_str(), FILE_ATTRIBUTE_NORMAL);
        }

        HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE | GENERIC_READ,
                                   FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                   FILE_FLAG_WRITE_THROUGH, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Cannot open file: " << filepath << " (Error " << GetLastError() << ")\n";
            return false;
        }

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) {
            std::cerr << "[-] Cannot query size for: " << filepath << "\n";
            CloseHandle(hFile);
            return false;
        }

        uint64_t total_bytes = static_cast<uint64_t>(file_size.QuadPart);
        const size_t BUFFER_SIZE = 65536;
        std::vector<char> buffer(BUFFER_SIZE);

        for (size_t p = 0; p < passes.size(); ++p) {
            if (verbose) {
                std::cout << "[*] Wiping " << filepath << " - Pass " << (p + 1) << "/" << passes.size() << "...\n";
            }

            LARGE_INTEGER zero = {0};
            SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);

            uint64_t bytes_written_total = 0;
            PassGenerator::FillBuffer(buffer, passes[p]);

            while (bytes_written_total < total_bytes) {
                DWORD to_write = static_cast<DWORD>((std::min)(static_cast<uint64_t>(BUFFER_SIZE), total_bytes - bytes_written_total));
                
                if (passes[p].type == PASS_RANDOM) {
                    PassGenerator::FillBuffer(buffer, passes[p]);
                }

                DWORD written = 0;
                if (!WriteFile(hFile, buffer.data(), to_write, &written, NULL) || written == 0) {
                    std::cerr << "[-] Write error on file: " << filepath << " (Error " << GetLastError() << ")\n";
                    CloseHandle(hFile);
                    return false;
                }
                bytes_written_total += written;
            }

            FlushFileBuffers(hFile);
        }

        LARGE_INTEGER zero = {0};
        SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
        CloseHandle(hFile);

        return SecureRenameAndDelete(filepath, verbose);
    }
