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

int runRpm(int argc, char* argv[]) {
    PersistentWalJournal::recoverInterruptedTransactions();

    if (argc <= 1) {
        std::cerr << "rpm: no operation specified\nTry 'rpm --help' for more information.\n";
        return 1;
    }

    bool opQuery = false, opInstall = false, opUpgrade = false, opErase = false, opVerify = false;
    bool optInfo = false, optList = false, optAll = false, optPackage = false, optHash = false;
    bool optVerbose = false, optNoDeps = false, optNoScripts = false, optTest = false;
    bool optWhatProvides = false, optWhatRequires = false;
    fs::path targetRoot = "C:\\";
    std::vector<std::string> arguments;

    // First pass: detect main mode from arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-q" || arg == "--query" || arg.rfind("-q", 0) == 0 ||
            arg == "-a" || arg == "--all" || arg == "-l" || arg == "--list" ||
            arg == "--info" || arg == "--whatprovides" || arg == "--whatrequires") {
            opQuery = true;
        } else if (arg == "-V" || arg == "--verify" || arg.rfind("-V", 0) == 0) {
            opVerify = true;
        } else if (arg == "-e" || arg == "--erase" || arg.rfind("-e", 0) == 0) {
            opErase = true;
        } else if (arg == "-U" || arg == "--upgrade" || arg.rfind("-U", 0) == 0) {
            opUpgrade = true;
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
            opQuery = true;
            optWhatProvides = true;
        } else if (arg == "--whatrequires") {
            opQuery = true;
            optWhatRequires = true;
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
                    case 'a': opQuery = true; optAll = true; break;
                    case 'l': opQuery = true; optList = true; break;
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
        else if (arg == "--info") { opQuery = true; optInfo = true; }
        else if (arg == "--list") { opQuery = true; optList = true; }
        else if (arg == "--all") { opQuery = true; optAll = true; }
        else if (arg == "--package") optPackage = true;
        else if (arg == "--hash") optHash = true;
        else if (arg == "--verbose") optVerbose = true;
        else arguments.push_back(arg);
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

        if (optAll) {
            for (const auto& [name, pkg] : db.all()) {
                std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
            }
            return 0;
        }

        if (arguments.empty()) {
            std::cerr << "error: no packages given for query\n";
            return 1;
        }

        int queryExitCode = 0;
        for (const auto& target : arguments) {
            RpmMetadata meta;
            if (optPackage) {
                RpmPackage pkg;
                if (!pkg.parse(target)) return 1;
                meta = pkg.meta;
            } else {
                RpmMetadata* found = db.find(target);
                if (!found) {
                    std::cerr << "package " << target << " is not installed\n";
                    queryExitCode = 1;
                    continue;
                }
                meta = *found;
            }

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
            } else if (optList) {
                for (const auto& f : meta.files) std::cout << f.path << "\n";
            } else {
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

                if (!optNoDeps && !DependencyEngine::checkEraseDependencies(pkgName, db)) {
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

                db.erase(pkgName);
                if (optVerbose) std::cout << "Erased: " << pkgName << "\n";
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
        std::vector<std::string> targets = arguments;

        if (optAll) {
            targets.clear();
            for (const auto& [name, pkg] : db.all()) {
                targets.push_back(name);
            }
        }

        if (targets.empty()) {
            std::cerr << "error: no packages given for verify\n";
            return 1;
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
