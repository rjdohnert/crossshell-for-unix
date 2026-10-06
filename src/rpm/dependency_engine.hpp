#pragma once

#include "dependency.hpp"
#include "rpm_database.hpp"
#include "rpm_metadata.hpp"
#include "rpm.hpp"

class DependencyEngine {
public:
    static bool matchDependency(const std::string& candidateVer, const Dependency& req);

    static bool verify(const RpmMetadata& pkg, RpmDatabase& db);

    static bool checkEraseDependencies(const std::string& pkgName, RpmDatabase& db);
};
