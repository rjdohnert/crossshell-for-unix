#include "archive_extraction_engine.hpp"
#include "cng_crypto_engine.hpp"
#include "dependency_engine.hpp"
#include "file_record.hpp"
#include "help_formatter.hpp"
#include "persistent_wal_journal.hpp"
#include "rpm_app.hpp"
#include "rpm_database.hpp"
#include "rpm_metadata.hpp"
#include "rpm_package.hpp"
#include "scriptlet_runner.hpp"
#include "security_engine.hpp"

namespace {
bool matchFilePath(const std::string& recordedPath, const std::string& queryPath) {
    if (recordedPath.empty() || queryPath.empty()) return false;

    std::string rec = recordedPath;
    std::string qry = queryPath;
    std::replace(rec.begin(), rec.end(), '/', '\\');
    std::replace(qry.begin(), qry.end(), '/', '\\');

    if (_stricmp(rec.c_str(), qry.c_str()) == 0) return true;

    std::error_code ec;
    if (fs::exists(rec, ec) && fs::exists(qry, ec)) {
        if (fs::equivalent(rec, qry, ec)) return true;
    }

    std::string recNoDrive = (rec.size() >= 2 && rec[1] == ':') ? rec.substr(2) : rec;
    std::string qryNoDrive = (qry.size() >= 2 && qry[1] == ':') ? qry.substr(2) : qry;

    while (!recNoDrive.empty() && recNoDrive.front() == '\\') recNoDrive.erase(recNoDrive.begin());
    while (!qryNoDrive.empty() && qryNoDrive.front() == '\\') qryNoDrive.erase(qryNoDrive.begin());

    if (_stricmp(recNoDrive.c_str(), qryNoDrive.c_str()) == 0) return true;

    if (!qryNoDrive.empty() && recNoDrive.size() >= qryNoDrive.size()) {
        size_t offset = recNoDrive.size() - qryNoDrive.size();
        if (_stricmp(recNoDrive.c_str() + offset, qryNoDrive.c_str()) == 0) {
            if (offset == 0 || recNoDrive[offset - 1] == '\\') return true;
        }
    }

    if (!recNoDrive.empty() && qryNoDrive.size() >= recNoDrive.size()) {
        size_t offset = qryNoDrive.size() - recNoDrive.size();
        if (_stricmp(qryNoDrive.c_str() + offset, recNoDrive.c_str()) == 0) {
            if (offset == 0 || qryNoDrive[offset - 1] == '\\') return true;
        }
    }

    return false;
}
}

int runRpm(int argc, char* argv[]) {
    PersistentWalJournal::recoverInterruptedTransactions();

    if (argc <= 1) {
        std::cerr << "rpm: no operation specified\nTry 'rpm --help' for more information.\n";
        return 1;
    }

    bool opQuery = false, opInstall = false, opUpgrade = false, opErase = false, opVerify = false;
    bool optInfo = false, optList = false, optAll = false, optPackage = false, optHash = false;
    bool optVerbose = false, optNoDeps = false, optNoScripts = false, optTest = false;
    bool optWhatProvides = false, optWhatRequires = false, optFile = false;
    fs::path targetRoot = "C:\\";
    std::vector<std::string> arguments;

    // First pass: detect main mode from arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-q" || arg == "--query" || arg.rfind("-q", 0) == 0) {
            opQuery = true;
        } else if (arg == "-V" || arg == "--verify" || arg.rfind("-V", 0) == 0) {
            opVerify = true;
        } else if (arg == "-e" || arg == "--erase" || arg.rfind("-e", 0) == 0) {
            opErase = true;
        } else if (arg == "-U" || arg == "--upgrade" || arg.rfind("-U", 0) == 0) {
            opUpgrade = true;
        } else if (arg == "--install") {
            opInstall = true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            printHelp();
            return 0;
        } else if (arg == "--version") {
            std::cout << "RPM version 3.8.19\n";
            return 0;
        } else if (arg == "--root" && i + 1 < argc) {
            targetRoot = argv[++i];
        } else if (arg.rfind("--root=", 0) == 0) {
            targetRoot = arg.substr(7);
        } else if (arg == "--prefix" && i + 1 < argc) {
            targetRoot = argv[++i];
        } else if (arg.rfind("--prefix=", 0) == 0) {
            targetRoot = arg.substr(9);
        } else if (arg == "--nodeps") {
            optNoDeps = true;
        } else if (arg == "--noscripts") {
            optNoScripts = true;
        } else if (arg == "--test") {
            optTest = true;
        } else if (arg == "--whatprovides") {
            if (!opVerify) opQuery = true;
            optWhatProvides = true;
        } else if (arg == "--whatrequires") {
            if (!opVerify) opQuery = true;
            optWhatRequires = true;
        } else if (arg == "--file") {
            optFile = true;
        } else if (arg.rfind("-", 0) == 0 && arg.rfind("--", 0) != 0) {
            for (size_t c = 1; c < arg.size(); ++c) {
                switch (arg[c]) {
                    case 'q': opQuery = true; break;
                    case 'i':
                        if (opQuery) optInfo = true;
                        else opInstall = true;
                        break;
                    case 'U': opUpgrade = true; break;
                    case 'e': opErase = true; break;
                    case 'V': opVerify = true; break;
                    case 'a': optAll = true; break;
                    case 'f': optFile = true; break;
                    case 'l': optList = true; break;
                    case 'p': optPackage = true; break;
                    case 'h': optHash = true; break;
                    case 'v': optVerbose = true; break;
                    default:
                        std::cerr << "rpm: invalid option -- '" << arg[c] << "'\n";
                        return 1;
                }
            }
        } else if (arg == "--query") opQuery = true;
        else if (arg == "--install") opInstall = true;
        else if (arg == "--upgrade") opUpgrade = true;
        else if (arg == "--erase") opErase = true;
        else if (arg == "--verify") opVerify = true;
        else if (arg == "--info") optInfo = true;
        else if (arg == "--list") optList = true;
        else if (arg == "--all") optAll = true;
        else if (arg == "--package") optPackage = true;
        else if (arg == "--hash") optHash = true;
        else if (arg == "--verbose") optVerbose = true;
        else arguments.push_back(arg);
    }

    if (!opInstall && !opUpgrade && !opErase && !opVerify) {
        opQuery = true;
    }

    RpmDatabase db(targetRoot);

    // ========================================================================
    // QUERY OPERATIONS
    // ========================================================================
    if (opQuery) {
        db.load();

        if (optWhatProvides) {
            if (arguments.empty()) {
                std::cerr << "error: no capability specified for --whatprovides\n";
                return 1;
            }
            int queryExitCode = 0;
            for (const auto& cap : arguments) {
                bool foundAny = false;
                for (const auto& [name, pkg] : db.all()) {
                    if (pkg.name == cap) {
                        std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                        foundAny = true;
                        continue;
                    }
                    for (const auto& prov : pkg.provides) {
                        if (prov.name == cap) {
                            std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                            foundAny = true;
                            break;
                        }
                    }
                    if (foundAny) continue;
                    for (const auto& f : pkg.files) {
                        if (matchFilePath(f.path, cap)) {
                            std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                            foundAny = true;
                            break;
                        }
                    }
                }
                if (!foundAny) {
                    std::cerr << "no package provides " << cap << "\n";
                    queryExitCode = 1;
                }
            }
            return queryExitCode;
        }

        if (optWhatRequires) {
            if (arguments.empty()) {
                std::cerr << "error: no capability specified for --whatrequires\n";
                return 1;
            }
            int queryExitCode = 0;
            for (const auto& cap : arguments) {
                bool foundAny = false;
                for (const auto& [name, pkg] : db.all()) {
                    for (const auto& req : pkg.requirements) {
                        if (req.name == cap) {
                            std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                            foundAny = true;
                            break;
                        }
                    }
                }
                if (!foundAny) {
                    std::cerr << "no package requires " << cap << "\n";
                    queryExitCode = 1;
                }
            }
            return queryExitCode;
        }

        std::vector<RpmMetadata> targetsToQuery;
        int queryExitCode = 0;

        if (optAll) {
            for (const auto& [name, pkg] : db.all()) {
                targetsToQuery.push_back(pkg);
            }
        } else if (optFile) {
            if (arguments.empty()) {
                std::cerr << "error: no files given for query\n";
                return 1;
            }
            for (const auto& filePath : arguments) {
                bool foundFile = false;
                for (const auto& [name, pkg] : db.all()) {
                    for (const auto& f : pkg.files) {
                        if (matchFilePath(f.path, filePath)) {
                            targetsToQuery.push_back(pkg);
                            foundFile = true;
                            break;
                        }
                    }
                    if (foundFile) break;
                }
                if (!foundFile) {
                    std::cerr << "file " << filePath << " is not owned by any package\n";
                    queryExitCode = 1;
                }
            }
        } else {
            if (arguments.empty()) {
                std::cerr << "error: no packages given for query\n";
                return 1;
            }

            for (const auto& target : arguments) {
                if (optPackage) {
                    RpmPackage pkg;
                    if (!pkg.parse(target)) {
                        queryExitCode = 1;
                        continue;
                    }
                    targetsToQuery.push_back(pkg.meta);
                } else {
                    RpmMetadata* found = db.find(target);
                    if (!found) {
                        std::cerr << "package " << target << " is not installed\n";
                        queryExitCode = 1;
                        continue;
                    }
                    targetsToQuery.push_back(*found);
                }
            }
        }

        for (const auto& meta : targetsToQuery) {
            if (optInfo) {
                std::cout << std::left << std::setw(15) << "Name" << ": " << meta.name << "\n";
                std::cout << std::left << std::setw(15) << "Version" << ": " << meta.version << "\n";
                std::cout << std::left << std::setw(15) << "Release" << ": " << meta.release << "\n";
                std::cout << std::left << std::setw(15) << "Architecture" << ": " << meta.arch << "\n";
                std::cout << std::left << std::setw(15) << "Group" << ": " << meta.group << "\n";
                std::cout << std::left << std::setw(15) << "Size" << ": " << meta.size << "\n";
                std::cout << std::left << std::setw(15) << "License" << ": " << meta.license << "\n";
                std::cout << std::left << std::setw(15) << "Summary" << ": " << meta.summary << "\n";
                std::cout << std::left << std::setw(15) << "Description" << ":\n" << meta.description << "\n";
            }
            if (optList) {
                for (const auto& f : meta.files) std::cout << f.path << "\n";
            }
            if (!optInfo && !optList) {
                std::cout << meta.name << "-" << meta.version << "-" << meta.release << "." << meta.arch << "\n";
            }
        }
        return queryExitCode;
    }

    // ========================================================================
    // MUTATION OPERATIONS (Install / Upgrade / Erase)
    // ========================================================================
    if (opInstall || opUpgrade || opErase) {
        bool isDefaultRoot = (targetRoot == "C:\\" || targetRoot == "C:/");
        if (!optTest && isDefaultRoot && !SecurityEngine::isAdministrator()) {
            std::cerr << "error: RPM operations require administrative privileges.\n";
            return 1;
        }

        if (!optTest) {
            if (!db.lock()) {
                std::cerr << "error: failed to acquire database lock\n";
                return 1;
            }
        }
        db.load();

        if (opInstall || opUpgrade) {
            if (arguments.empty()) {
                std::cerr << "error: no packages given for install\n";
                db.unlock();
                return 1;
            }

            for (const auto& rpmFilePath : arguments) {
                RpmPackage pkg;
                if (!pkg.parse(rpmFilePath)) {
                    db.unlock();
                    return 1;
                }

                RpmMetadata* oldPkg = db.find(pkg.meta.name);
                if (oldPkg && !opUpgrade) {
                    std::cerr << "package " << pkg.meta.name << " is already installed\n";
                    db.unlock();
                    return 1;
                }

                if (!optNoDeps && !DependencyEngine::verify(pkg.meta, db)) {
                    db.unlock();
                    return 1;
                }

                if (optTest) {
                    std::cout << "Test mode: package " << pkg.meta.name << " can be installed cleanly.\n";
                    continue;
                }

                PersistentWalJournal journal(targetRoot);

                if (!optNoScripts && !pkg.meta.preInScript.empty()) {
                    if (!ScriptletRunner::execute(pkg.meta.preInScript, "prein")) {
                        std::cerr << "error: %prein scriptlet failed.\n";
                        journal.rollback();
                        db.unlock();
                        return 1;
                    }
                }

                std::vector<FileRecord> extractedFiles;
                if (!ArchiveExtractionEngine::extract(rpmFilePath, pkg.payloadOffset, targetRoot, journal, extractedFiles, pkg.meta.files)) {
                    std::cerr << "error: archive extraction failed for " << rpmFilePath << "\n";
                    journal.rollback();
                    db.unlock();
                    return 1;
                }
                pkg.meta.files = extractedFiles;

                if (!optNoScripts && !pkg.meta.postInScript.empty()) {
                    if (!ScriptletRunner::execute(pkg.meta.postInScript, "postin")) {
                        std::cerr << "error: %postin scriptlet failed. Rolling back transaction.\n";
                        journal.rollback();
                        db.unlock();
                        return 1;
                    }
                }

                // If upgrading, run old package %preun, clean up obsolete files, and run %postun
                if (oldPkg) {
                    if (!optNoScripts && !oldPkg->preUnScript.empty()) {
                        ScriptletRunner::execute(oldPkg->preUnScript, "preun");
                    }
                    std::set<std::string> newPaths;
                    for (const auto& f : pkg.meta.files) newPaths.insert(f.path);
                    for (const auto& f : oldPkg->files) {
                        if (newPaths.count(f.path) == 0) {
                            std::error_code ec;
                            fs::remove(f.path, ec);
                        }
                    }
                    if (!optNoScripts && !oldPkg->postUnScript.empty()) {
                        ScriptletRunner::execute(oldPkg->postUnScript, "postun");
                    }
                }

                journal.commit();
                db.add(pkg.meta);

                if (optHash) {
                    printProgress(pkg.meta.name);
                } else if (optVerbose) {
                    std::cout << (oldPkg ? "Upgraded: " : "Installed: ")
                              << pkg.meta.name << "-" << pkg.meta.version << "-" << pkg.meta.release << "\n";
                }
            }
        } else if (opErase) {
            if (arguments.empty()) {
                std::cerr << "error: no packages given for erase\n";
                db.unlock();
                return 1;
            }

            for (const auto& pkgName : arguments) {
                RpmMetadata* found = db.find(pkgName);
                if (!found) {
                    std::cerr << "error: package " << pkgName << " is not installed\n";
                    db.unlock();
                    return 1;
                }
                std::string canonicalName = found->name;

                if (!optNoDeps && !DependencyEngine::checkEraseDependencies(canonicalName, db)) {
                    db.unlock();
                    return 1;
                }

                if (!optNoScripts && !found->preUnScript.empty()) {
                    ScriptletRunner::execute(found->preUnScript, "preun");
                }

                for (const auto& fileRec : found->files) {
                    std::error_code ec;
                    fs::remove(fileRec.path, ec);
                }

                if (!optNoScripts && !found->postUnScript.empty()) {
                    ScriptletRunner::execute(found->postUnScript, "postun");
                }

                db.erase(canonicalName);
                if (optVerbose) std::cout << "Erased: " << canonicalName << "\n";
            }
        }
        db.unlock();
        return 0;
    }

    // ========================================================================
    // VERIFY OPERATIONS
    // ========================================================================
    if (opVerify) {
        db.load();
        int verifyExitCode = 0;
        std::vector<std::string> targets;

        if (optAll) {
            for (const auto& [name, pkg] : db.all()) {
                targets.push_back(name);
            }
        } else if (optFile) {
            if (arguments.empty()) {
                std::cerr << "error: no files given for verify\n";
                return 1;
            }
            for (const auto& filePath : arguments) {
                bool foundFile = false;
                for (const auto& [name, pkg] : db.all()) {
                    for (const auto& f : pkg.files) {
                        if (matchFilePath(f.path, filePath)) {
                            targets.push_back(pkg.name);
                            foundFile = true;
                            break;
                        }
                    }
                    if (foundFile) break;
                }
                if (!foundFile) {
                    std::cerr << "file " << filePath << " is not owned by any package\n";
                    verifyExitCode = 1;
                }
            }
        } else {
            targets = arguments;
        }

        if (targets.empty()) {
            if (verifyExitCode == 0) {
                std::cerr << "error: no packages given for verify\n";
                return 1;
            }
            return verifyExitCode;
        }

        for (const auto& pkgName : targets) {
            RpmMetadata* found = db.find(pkgName);
            if (!found) {
                std::cerr << "package " << pkgName << " is not installed\n";
                verifyExitCode = 1;
                continue;
            }

            for (const auto& fileRec : found->files) {
                std::error_code ec;
                if (!fs::exists(fileRec.path, ec)) {
                    std::cout << "missing     " << fileRec.path << "\n";
                    verifyExitCode = 1;
                } else if (!fileRec.sha256.empty()) {
                    std::string currentHash = CngCryptoEngine::calculateFileSha256(fileRec.path);
                    if (currentHash != fileRec.sha256) {
                        std::cout << "..5......   " << fileRec.path << " (digest mismatch)\n";
                        verifyExitCode = 1;
                    }
                }
            }
        }
        return verifyExitCode;
    }

    return 0;
}
