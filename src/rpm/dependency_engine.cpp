#include "dependency_engine.hpp"
#include "dependency.hpp"
#include "evr_engine.hpp"
#include "rpm_database.hpp"
#include "rpm_format.hpp"
#include "rpm_metadata.hpp"

bool DependencyEngine::matchDependency(const std::string& candidateVer, const Dependency& req) {
        if (req.version.empty() || req.flags == 0) return true;

        int cmp = EvrEngine::rpmvercmp(candidateVer, req.version);
        if (cmp == 0 && (req.flags & RPMSENSE_EQUAL)) return true;
        if (cmp > 0 && (req.flags & RPMSENSE_GREATER)) return true;
        if (cmp < 0 && (req.flags & RPMSENSE_LESS)) return true;

        return false;
    }

bool DependencyEngine::verify(const RpmMetadata& pkg, RpmDatabase& db) {
        for (const auto& req : pkg.requirements) {
            if (req.name.rfind("rpmlib(", 0) == 0) continue;

            bool satisfied = false;
            for (const auto& [name, installed] : db.all()) {
                // Match direct package name
                if (installed.name == req.name) {
                    if (matchDependency(installed.version, req)) {
                        satisfied = true;
                        break;
                    }
                }

                // Match provides virtual capabilities
                for (const auto& prov : installed.provides) {
                    if (prov.name == req.name) {
                        std::string provVer = prov.version.empty() ? installed.version : prov.version;
                        if (matchDependency(provVer, req)) {
                            satisfied = true;
                            break;
                        }
                    }
                }
                if (satisfied) break;
            }

            if (!satisfied) {
                std::cerr << "error: Failed dependencies:\n";
                std::cerr << "  " << req.name << " " << (req.version.empty() ? "" : "= " + req.version)
                          << " is needed by " << pkg.name << "-" << pkg.version << "-" << pkg.release << "\n";
                return false;
            }
        }
        return true;
    }

bool DependencyEngine::checkEraseDependencies(const std::string& pkgName, RpmDatabase& db) {
        RpmMetadata* found = db.find(pkgName);
        if (!found) return true;

        std::set<std::string> providedCaps;
        providedCaps.insert(pkgName);
        for (const auto& prov : found->provides) {
            providedCaps.insert(prov.name);
        }

        for (const auto& [name, installed] : db.all()) {
            if (installed.name == pkgName) continue;
            for (const auto& req : installed.requirements) {
                if (providedCaps.count(req.name) > 0) {
                    // Check if another installed package still provides this requirement
                    bool stillProvided = false;
                    for (const auto& [otherName, otherPkg] : db.all()) {
                        if (otherName == pkgName) continue;
                        if (otherPkg.name == req.name && matchDependency(otherPkg.version, req)) {
                            stillProvided = true;
                            break;
                        }
                        for (const auto& prov : otherPkg.provides) {
                            if (prov.name == req.name) {
                                std::string provVer = prov.version.empty() ? otherPkg.version : prov.version;
                                if (matchDependency(provVer, req)) {
                                    stillProvided = true;
                                    break;
                                }
                            }
                        }
                        if (stillProvided) break;
                    }

                    if (!stillProvided) {
                        std::cerr << "error: Failed dependencies:\n";
                        std::cerr << "  " << req.name << " is needed by (installed) "
                                  << installed.name << "-" << installed.version << "-" << installed.release << "\n";
                        return false;
                    }
                }
            }
        }
        return true;
    }
