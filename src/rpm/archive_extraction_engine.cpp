#include "archive_extraction_engine.hpp"
#include "file_record.hpp"
#include "persistent_wal_journal.hpp"

fs::path ArchiveExtractionEngine::sanitizePath(const fs::path& root, const std::string& raw) {
        std::string s = raw;
        std::replace(s.begin(), s.end(), '/', '\\');

        // Remove drive specifiers if any
        if (s.size() >= 2 && s[1] == ':') {
            s = s.substr(2);
        }

        // Strip leading slashes and relative current directory prefix
        while (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
        if (s.rfind(".\\", 0) == 0) s = s.substr(2);

        // Disallow path traversal sequences
        fs::path relPath;
        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, '\\')) {
            if (token.empty() || token == ".") continue;
            if (token == "..") continue; // Prevent directory traversal
            relPath /= token;
        }

        return root / relPath;
    }

bool ArchiveExtractionEngine::extract(const fs::path& rpmPath, std::streampos offset, const fs::path& root,
                        PersistentWalJournal& journal, std::vector<FileRecord>& outFiles,
                        const std::vector<FileRecord>& headerFiles) {
        std::ifstream file(rpmPath, std::ios::binary);
        if (!file.is_open()) return false;

        file.seekg(0, std::ios::end);
        std::streampos endPos = file.tellg();
        if (endPos <= offset) return false;

        size_t totalSize = static_cast<size_t>(endPos - offset);
        file.seekg(offset);

        std::vector<char> buffer(totalSize);
        file.read(buffer.data(), totalSize);

#if defined(USE_LIBARCHIVE)
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (archive_read_open_memory(a, buffer.data(), buffer.size()) != ARCHIVE_OK) {
            archive_read_free(a);
            return false;
        }

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            std::string pathStr = archive_entry_pathname(entry);
            fs::path targetPath = sanitizePath(root, pathStr);
            size_t size = archive_entry_size(entry);
            mode_t mode = archive_entry_mode(entry);

            if (archive_entry_filetype(entry) == AE_IFDIR) {
                std::error_code ec;
                fs::create_directories(targetPath, ec);
            } else {
                std::vector<char> fileData(size);
                archive_read_data(a, fileData.data(), size);
                std::string computedHash;
                if (!journal.safeWrite(targetPath, fileData, static_cast<uint32_t>(mode), &computedHash)) {
                    archive_read_free(a);
                    return false;
                }
                FileRecord rec;
                rec.path = targetPath.string();
                rec.size = static_cast<uint32_t>(size);
                rec.mode = static_cast<uint32_t>(mode);
                rec.sha256 = computedHash;
                outFiles.push_back(rec);
            }
        }
        archive_read_free(a);
        return true;
#else
        // Check if uncompressed CPIO
        bool isUncompressedCpio = false;
        if (buffer.size() >= 6) {
            if (std::memcmp(buffer.data(), "070701", 6) == 0 || std::memcmp(buffer.data(), "070702", 6) == 0) {
                isUncompressedCpio = true;
            }
        }

        if (isUncompressedCpio) {
            #pragma pack(push, 1)
            struct CpioNewc {
                char magic[6]; char ino[8]; char mode[8]; char uid[8]; char gid[8];
                char nlink[8]; char mtime[8]; char filesize[8]; char devmajor[8];
                char devminor[8]; char rdevmajor[8]; char rdevminor[8];
                char namesize[8]; char check[8];
            };
            #pragma pack(pop)

            auto parseHex = [](const char* p, size_t n) -> uint32_t {
                std::string s(p, n);
                return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 16));
            };

            const char* ptr = buffer.data();
            const char* end = ptr + buffer.size();
            bool trailerFound = false;

            while (ptr + sizeof(CpioNewc) <= end) {
                const CpioNewc* hdr = reinterpret_cast<const CpioNewc*>(ptr);
                if (std::memcmp(hdr->magic, "070701", 6) != 0 && std::memcmp(hdr->magic, "070702", 6) != 0) {
                    break;
                }

                uint32_t mode     = parseHex(hdr->mode, 8);
                uint32_t filesize = parseHex(hdr->filesize, 8);
                uint32_t namesize = parseHex(hdr->namesize, 8);

                ptr += sizeof(CpioNewc);
                if (ptr + namesize > end) return false;

                std::string filename(ptr, namesize > 0 ? namesize - 1 : 0);
                ptr += namesize;

                size_t padName = (4 - ((sizeof(CpioNewc) + namesize) % 4)) % 4;
                ptr += padName;

                if (filename == "TRAILER!!!") {
                    trailerFound = true;
                    break;
                }
                if (filename.empty()) continue;

                fs::path dest = sanitizePath(root, filename);
                bool isDir = (mode & 0040000) != 0;

                if (isDir) {
                    std::error_code ec;
                    fs::create_directories(dest, ec);
                } else {
                    if (ptr + filesize > end) return false;
                    std::vector<char> fileData(ptr, ptr + filesize);
                    ptr += filesize;

                    std::string computedHash;
                    if (!journal.safeWrite(dest, fileData, mode, &computedHash)) return false;

                    FileRecord rec;
                    rec.path = dest.string();
                    rec.size = filesize;
                    rec.mode = mode;
                    rec.sha256 = computedHash;
                    outFiles.push_back(rec);
                }

                size_t padData = (4 - (filesize % 4)) % 4;
                ptr += padData;
            }
            if (!trailerFound && !headerFiles.empty() && outFiles.empty()) {
                return false;
            }
            return true;
        }

        // Compressed payload (gzip/xz/zstd/bzip2) fallback using Windows native tar.exe (bsdtar)
        fs::path tempPayload = fs::temp_directory_path() / ("rpm_payload_" + std::to_string(GetCurrentProcessId()) + ".bin");
        fs::path tempExtract = fs::temp_directory_path() / ("rpm_extract_" + std::to_string(GetCurrentProcessId()));

        {
            std::ofstream out(tempPayload, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(buffer.data(), buffer.size());
        }

        std::error_code ec;
        fs::create_directories(tempExtract, ec);

        std::wstring tarCmd = L"tar.exe -xf \"" + tempPayload.wstring() + L"\" -C \"" + tempExtract.wstring() + L"\"";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(tarCmd.begin(), tarCmd.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        bool tarSuccess = false;
        if (ok) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 1;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0) {
                tarSuccess = true;
                for (const auto& entry : fs::recursive_directory_iterator(tempExtract, ec)) {
                    fs::path rel = fs::relative(entry.path(), tempExtract, ec);
                    fs::path dest = sanitizePath(root, rel.string());

                    if (entry.is_directory(ec)) {
                        fs::create_directories(dest, ec);
                    } else if (entry.is_regular_file(ec)) {
                        std::ifstream in(entry.path(), std::ios::binary);
                        std::vector<char> fileData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                        in.close();

                        // Look up true mode from headerFiles if available
                        uint32_t mode = 0100644;
                        std::string relGen = rel.generic_string();
                        for (const auto& hf : headerFiles) {
                            fs::path hfPath(hf.path);
                            if (hfPath.generic_string() == relGen ||
                                hfPath.filename() == rel.filename() ||
                                hf.path.find(relGen) != std::string::npos) {
                                mode = hf.mode;
                                break;
                            }
                        }
                        if (mode == 0) mode = 0100644;

                        std::string computedHash;
                        if (!journal.safeWrite(dest, fileData, mode, &computedHash)) {
                            fs::remove(tempPayload, ec);
                            fs::remove_all(tempExtract, ec);
                            return false;
                        }

                        FileRecord rec;
                        rec.path = dest.string();
                        rec.size = static_cast<uint32_t>(fileData.size());
                        rec.mode = mode;
                        rec.sha256 = computedHash;
                        outFiles.push_back(rec);
                    }
                }
            }
        }

        fs::remove(tempPayload, ec);
        fs::remove_all(tempExtract, ec);

        if (!tarSuccess) {
            return false;
        }

        if (outFiles.empty() && !headerFiles.empty()) {
            return false;
        }
        return true;
#endif
    }
