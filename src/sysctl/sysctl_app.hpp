#pragma once

#include "mib_tree.hpp"
#include "options.hpp"
#include <string>
#include <vector>

class SysctlApp {
public:
    SysctlApp();
    explicit SysctlApp(MibTree tree);

    int Run(int argc, char* argv[]);

    bool ProcessLine(const std::string& line);
    bool ProcessQuery(const std::string& line);

private:
    void EnsureStorage();
    bool UnknownName(const std::string& key) const;

    MibTree m_tree;
    CliOptions m_opts;
    std::vector<std::string> m_queries;
    bool m_storagePopulated = false;
};
