#include "cng_crypto_engine.hpp"
#include "persistent_wal_journal.hpp"
#include "security_engine.hpp"

PersistentWalJournal::PersistentWalJournal(const fs::path& root ) {
        if (!root.empty() && root != "C:\\" && root != "C:/") {
            journalDir = root / "ProgramData" / "RPM" / "journal";
        } else {
            char path[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
                journalDir = fs::path(path) / "RPM" / "journal";
            } else {
                journalDir = "C:\\ProgramData\\RPM\\journal";
            }
        }
        std::error_code ec;
        fs::create_directories(journalDir, ec);
        stagingDir = journalDir / ("tx_" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(stagingDir, ec);

        activeWalFile = journalDir / ("tx_" + std::to_string(GetCurrentProcessId()) + ".wal");
        walStream.open(activeWalFile, std::ios::out | std::ios::app);
    }

PersistentWalJournal::~PersistentWalJournal() {
        if (walStream.is_open()) walStream.close();
        std::error_code ec;
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }

void PersistentWalJournal::recoverInterruptedTransactions() {
        char path[MAX_PATH];
        fs::path jDir = "C:\\ProgramData\\RPM\\journal";
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
            jDir = fs::path(path) / "RPM" / "journal";
        }
        std::error_code ec;
        if (!fs::exists(jDir, ec)) return;

        for (const auto& entry : fs::directory_iterator(jDir, ec)) {
            if (entry.path().extension() == ".wal") {
                std::string fname = entry.path().stem().string();
                if (fname.rfind("tx_", 0) == 0) {
                    try {
                        DWORD pid = std::stoul(fname.substr(3));
                        if (pid > 0) {
                            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                            if (hProc != NULL) {
                                DWORD exitCode = 0;
                                if (GetExitCodeProcess(hProc, &exitCode) && exitCode == STILL_ACTIVE) {
                                    CloseHandle(hProc);
                                    continue; // Process is still actively running
                                }
                                CloseHandle(hProc);
                            }
                        }
                    } catch (...) {}
                }

                std::cerr << "rpm: Interrupted transaction found: " << entry.path().filename().string()
                          << ". Performing crash recovery rollback...\n";
                std::ifstream wal(entry.path());
                std::string line;
                std::vector<std::string> ops;
                while (std::getline(wal, line)) {
                    if (!line.empty()) ops.push_back(line);
                }
                wal.close();

                for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
                    auto sep = it->find(':');
                    if (sep == std::string::npos) continue;
                    std::string act = it->substr(0, sep);
                    std::string payload = it->substr(sep + 1);

                    if (act == "CREATE") {
                        fs::remove(payload, ec);
                    } else if (act == "REPLACE") {
                        auto bar = payload.find('|');
                        if (bar != std::string::npos) {
                            fs::path target = payload.substr(0, bar);
                            fs::path backup = payload.substr(bar + 1);
                            if (fs::exists(backup, ec)) {
                                fs::copy_file(backup, target, fs::copy_options::overwrite_existing, ec);
                            }
                        }
                    }
                }
                fs::remove(entry.path(), ec);
                fs::path stg = entry.path().parent_path() / entry.path().stem();
                fs::remove_all(stg, ec);
            }
        }
    }

bool PersistentWalJournal::safeWrite(const fs::path& dest, const std::vector<char>& data, uint32_t posixMode, std::string* outSha256 ) {
        std::error_code ec;
        if (fs::exists(dest, ec)) {
            fs::path backup = stagingDir / ("bak_" + std::to_string(loggedActions.size()));
            fs::copy_file(dest, backup, fs::copy_options::overwrite_existing, ec);
            walStream << "REPLACE:" << dest.string() << "|" << backup.string() << "\n";
            walStream.flush();
            loggedActions.push_back({WalAction::FileReplaced, {dest, backup}});
        } else {
            if (dest.has_parent_path()) fs::create_directories(dest.parent_path(), ec);
            walStream << "CREATE:" << dest.string() << "\n";
            walStream.flush();
            loggedActions.push_back({WalAction::FileCreated, {dest, ""}});
        }

        HANDLE hFile = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED) {
                fs::path pending = dest.string() + ".rpmsave";
                std::ofstream out(pending, std::ios::binary);
                if (out.is_open()) {
                    out.write(data.data(), data.size());
                    out.close();
                    if (outSha256) {
                        *outSha256 = CngCryptoEngine::calculateFileSha256(pending);
                    }
                    MoveFileExW(pending.c_str(), dest.c_str(), MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_REPLACE_EXISTING);
                    return true;
                }
            }
            return false;
        }

        DWORD written = 0;
        BOOL ok = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, NULL);
        CloseHandle(hFile);

        if (ok && (written == data.size())) {
            SecurityEngine::applyPosixPermissions(dest, posixMode);
            if (outSha256) {
                *outSha256 = CngCryptoEngine::calculateFileSha256(dest);
            }
            return true;
        }
        return false;
    }

void PersistentWalJournal::rollback() {
        if (walStream.is_open()) walStream.close();
        std::error_code ec;
        for (auto it = loggedActions.rbegin(); it != loggedActions.rend(); ++it) {
            if (it->first == WalAction::FileCreated) {
                fs::remove(it->second.first, ec);
            } else if (it->first == WalAction::FileReplaced) {
                if (fs::exists(it->second.second, ec)) {
                    fs::copy_file(it->second.second, it->second.first, fs::copy_options::overwrite_existing, ec);
                }
            }
        }
        loggedActions.clear();
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }

void PersistentWalJournal::commit() {
        if (walStream.is_open()) walStream.close();
        loggedActions.clear();
        std::error_code ec;
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }
