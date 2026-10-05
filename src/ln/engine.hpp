#pragma once

#include "ln.hpp"

class PrivilegeManager {
public:
    static bool EnableSymlinkPrivilege();
};

class PathInspector {
public:
    static bool PathExists(const fs::path& p);
    static bool IsDirectoryPath(const fs::path& p, bool no_deref);
};

class ConsolePrompter {
public:
    static bool ConfirmOverwrite(const fs::path& target);
};

class LinkEngine {
private:
    LnOptions m_opts;

public:
    explicit LinkEngine(LnOptions opts);
    bool CreateOneLink(const fs::path& source, const fs::path& target) const;
};
