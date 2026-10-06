#include "base64_codec.hpp"
#include "dependency.hpp"
#include "file_record.hpp"
#include "rpm_database.hpp"
#include "rpm_metadata.hpp"

RpmDatabase::RpmDatabase(const fs::path& root ) {
        if (!root.empty() && root != "C:\\" && root != "C:/") {
            dbDir = root / "ProgramData" / "RPM";
        } else {
            char path[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
                dbDir = fs::path(path) / "RPM";
            } else {
                dbDir = "C:\\ProgramData\\RPM";
            }
        }
        std::error_code ec;
        fs::create_directories(dbDir, ec);
        manifestPath = dbDir / "rpmdb.json";
        lockPath = dbDir / ".rpmdb.lock";
    }

bool RpmDatabase::lock() {
        std::error_code ec;
        fs::create_directories(dbDir, ec);
        hLock = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hLock == INVALID_HANDLE_VALUE) return false;
        OVERLAPPED ov = {0};
        if (!LockFileEx(hLock, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov)) {
            CloseHandle(hLock);
            hLock = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

void RpmDatabase::unlock() {
        if (hLock != INVALID_HANDLE_VALUE) {
            OVERLAPPED ov = {0};
            UnlockFileEx(hLock, 0, MAXDWORD, MAXDWORD, &ov);
            CloseHandle(hLock);
            hLock = INVALID_HANDLE_VALUE;
        }
    }

void RpmDatabase::load() {
        packages.clear();
        std::error_code ec;
        if (!fs::exists(manifestPath, ec)) return;

        std::ifstream file(manifestPath);
        if (!file.is_open()) return;

        std::string line;
        RpmMetadata curr;
        bool inPkg = false;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line == "PKG_START") {
                curr = RpmMetadata();
                inPkg = true;
            } else if (line == "PKG_END") {
                if (inPkg && !curr.name.empty()) packages[curr.name] = curr;
                inPkg = false;
            } else if (inPkg) {
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);

                if (k == "name") curr.name = v;
                else if (k == "version") curr.version = v;
                else if (k == "release") curr.release = v;
                else if (k == "epoch") curr.epoch = v;
                else if (k == "arch") curr.arch = v;
                else if (k == "summary") curr.summary = base64Decode(v);
                else if (k == "description") curr.description = base64Decode(v);
                else if (k == "vendor") curr.vendor = base64Decode(v);
                else if (k == "license") curr.license = v;
                else if (k == "group") curr.group = v;
                else if (k == "url") curr.url = v;
                else if (k == "os") curr.os = v;
                else if (k == "prein") curr.preInScript = base64Decode(v);
                else if (k == "postin") curr.postInScript = base64Decode(v);
                else if (k == "preun") curr.preUnScript = base64Decode(v);
                else if (k == "postun") curr.postUnScript = base64Decode(v);
                else if (k == "size") {
                    try { curr.size = static_cast<uint32_t>(std::stoul(v)); } catch (...) {}
                } else if (k == "buildtime") {
                    try { curr.buildTime = static_cast<uint32_t>(std::stoul(v)); } catch (...) {}
                } else if (k == "file") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            auto c3 = v.find('|', c2 + 1);
                            FileRecord fr;
                            fr.path = v.substr(0, c1);
                            fr.sha256 = v.substr(c1 + 1, c2 - c1 - 1);
                            try {
                                if (c3 != std::string::npos) {
                                    fr.size = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1, c3 - c2 - 1)));
                                    fr.mode = static_cast<uint32_t>(std::stoul(v.substr(c3 + 1)));
                                } else {
                                    fr.size = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1)));
                                    fr.mode = 0644;
                                }
                            } catch (...) {}
                            curr.files.push_back(fr);
                        }
                    }
                } else if (k == "require") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            Dependency dep;
                            dep.name = v.substr(0, c1);
                            dep.version = v.substr(c1 + 1, c2 - c1 - 1);
                            try { dep.flags = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1))); } catch (...) {}
                            curr.requirements.push_back(dep);
                        }
                    }
                } else if (k == "provide") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            Dependency dep;
                            dep.name = v.substr(0, c1);
                            dep.version = v.substr(c1 + 1, c2 - c1 - 1);
                            try { dep.flags = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1))); } catch (...) {}
                            curr.provides.push_back(dep);
                        }
                    }
                }
            }
        }
    }

void RpmDatabase::save() {
        fs::path tmp = manifestPath.string() + ".tmp";
        std::ofstream file(tmp, std::ios::trunc);
        for (const auto& [name, pkg] : packages) {
            file << "PKG_START\n";
            file << "name=" << pkg.name << "\n";
            file << "version=" << pkg.version << "\n";
            file << "release=" << pkg.release << "\n";
            file << "epoch=" << pkg.epoch << "\n";
            file << "arch=" << pkg.arch << "\n";
            file << "summary=" << base64Encode(pkg.summary) << "\n";
            file << "description=" << base64Encode(pkg.description) << "\n";
            file << "vendor=" << base64Encode(pkg.vendor) << "\n";
            file << "license=" << pkg.license << "\n";
            file << "group=" << pkg.group << "\n";
            file << "url=" << pkg.url << "\n";
            file << "os=" << pkg.os << "\n";
            file << "prein=" << base64Encode(pkg.preInScript) << "\n";
            file << "postin=" << base64Encode(pkg.postInScript) << "\n";
            file << "preun=" << base64Encode(pkg.preUnScript) << "\n";
            file << "postun=" << base64Encode(pkg.postUnScript) << "\n";
            file << "size=" << pkg.size << "\n";
            file << "buildtime=" << pkg.buildTime << "\n";

            for (const auto& f : pkg.files) {
                file << "file=" << f.path << "|" << f.sha256 << "|" << f.size << "|" << f.mode << "\n";
            }
            for (const auto& req : pkg.requirements) {
                file << "require=" << req.name << "|" << req.version << "|" << req.flags << "\n";
            }
            for (const auto& prov : pkg.provides) {
                file << "provide=" << prov.name << "|" << prov.version << "|" << prov.flags << "\n";
            }
            file << "PKG_END\n";
        }
        file.close();
        MoveFileExW(tmp.c_str(), manifestPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }

bool RpmDatabase::isInstalled(const std::string& name) { return packages.find(name) != packages.end(); }

RpmMetadata* RpmDatabase::find(const std::string& name) {
        auto it = packages.find(name);
        return it != packages.end() ? &it->second : nullptr;
    }

const std::map<std::string, RpmMetadata>& RpmDatabase::all() const { return packages; }

void RpmDatabase::add(const RpmMetadata& pkg) { packages[pkg.name] = pkg; save(); }

bool RpmDatabase::erase(const std::string& name) {
        if (packages.erase(name) > 0) { save(); return true; }
        return false;
    }
