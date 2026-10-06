#pragma once

#include "rpm_metadata.hpp"
#include "rpm.hpp"

class RpmDatabase {
private:
    fs::path dbDir;
    fs::path manifestPath;
    fs::path lockPath;
    HANDLE hLock = INVALID_HANDLE_VALUE;
    std::map<std::string, RpmMetadata> packages;

public:
    RpmDatabase(const fs::path& root = "");

    bool lock();

    void unlock();

    void load();

    void save();

    bool isInstalled(const std::string& name);
    RpmMetadata* find(const std::string& name);
    const std::map<std::string, RpmMetadata>& all() const;
    void add(const RpmMetadata& pkg);
    bool erase(const std::string& name);
};
