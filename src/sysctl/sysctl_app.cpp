#include "sysctl_app.hpp"
#include "telemetry_provider.hpp"
#include "storage_provider.hpp"
#include "reporter.hpp"
#include <iostream>
#include <fstream>

using namespace sysctl;

SysctlApp::SysctlApp() {
    WindowsTelemetryFactory::Populate(m_tree);
}

SysctlApp::SysctlApp(MibTree tree) : m_tree(std::move(tree)), m_storagePopulated(true) {}

void SysctlApp::EnsureStorage() {
    if (!m_storagePopulated) {
        WindowsStorageFactory::Populate(m_tree);
        m_storagePopulated = true;
    }
}

bool SysctlApp::UnknownName(const std::string& key) const {
    if (m_opts.ignoreUnknown) return true;
    const std::string top = key.substr(0, key.find('.'));
    if (top.empty() || !m_tree.FindNode(top)) {
        std::cerr << "sysctl: top level name " << top << " in " << key << " is invalid\n";
    } else {
        size_t start = top.size() + 1;
        while (start <= key.size()) {
            const auto end = key.find('.', start);
            if (!m_tree.FindNode(key.substr(0, end))) {
                std::cerr << "sysctl: name " << key.substr(start, end - start)
                          << " in " << key << " is unknown\n";
                break;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return false;
}

bool SysctlApp::ProcessLine(const std::string& line) {
    try {
        return ProcessQuery(line);
    } catch (const std::exception& ex) {
        std::cerr << "sysctl: " << Trim(line.substr(0, line.find('='))) << ": " << ex.what() << "\n";
        return false;
    }
}

bool SysctlApp::ProcessQuery(const std::string& line) {
    if (line.empty()) {
        std::cerr << "sysctl: empty variable name\n";
        return false;
    }

    size_t eqPos = line.find('=');
    if (eqPos != std::string::npos) {
        // Write Command
        std::string key = Trim(line.substr(0, eqPos));
        std::string val = Trim(line.substr(eqPos + 1));
        if (key.empty()) {
            std::cerr << "sysctl: empty variable name\n";
            return false;
        }

        if (key == "hw" || key.compare(0, 7, "hw.disk") == 0 ||
            key == "fs" || key.compare(0, 3, "fs.") == 0) EnsureStorage();
        auto node = m_tree.FindNode(key);
        if (!node) {
            return UnknownName(key);
        }
        if (node->IsDirectory()) {
            std::cerr << "sysctl: " << key << ": is a directory node, not a value\n";
            return false;
        }

        auto leaf = std::static_pointer_cast<SysctlLeafNode>(node);
        if (!leaf->IsWritable()) {
            std::cerr << "sysctl: " << key << ": Operation not permitted (read-only node)\n";
            return false;
        }

        std::string oldVal = leaf->GetValue();
        if (leaf->SetValue(val)) {
            if (!m_opts.quiet) {
                if (m_opts.nameOnly) {
                    std::cout << key << "\n";
                } else if (m_opts.valueOnly) {
                    std::cout << val << "\n";
                } else {
                    std::cout << key << ": " << oldVal << " -> " << val << "\n";
                }
            }
            return true;
        } else {
            std::cerr << "sysctl: " << key << ": failed to set value\n";
            return false;
        }
    } else {
        // Read Command
        std::string queryKey = line;
        if (queryKey.size() > 2 && queryKey.compare(queryKey.size() - 2, 2, ".*") == 0) {
            queryKey = queryKey.substr(0, queryKey.size() - 2);
        }

        if (queryKey == "hw" || queryKey.compare(0, 7, "hw.disk") == 0 ||
            queryKey == "fs" || queryKey.compare(0, 3, "fs.") == 0) EnsureStorage();
        auto node = m_tree.FindNode(queryKey);
        if (!node) {
            return UnknownName(line);
        }
        SysctlPrinterVisitor visitor(m_opts, node->IsDirectory());
        node->Accept(visitor);
        return visitor.Succeeded();
    }
}

int SysctlApp::Run(int argc, char* argv[]) {
    m_opts = {};
    m_queries.clear();
    if (!m_opts.Parse(argc, argv, m_queries)) {
        return 1;
    }
    if (m_opts.help) {
        m_opts.PrintHelp();
        return 0;
    }
    bool success = true;

    if (m_opts.listAll || (m_queries.empty() && m_opts.inputFile.empty())) {
        try {
            EnsureStorage();
        } catch (const std::exception& ex) {
            std::cerr << "sysctl: storage discovery: " << ex.what() << "\n";
            success = false;
        }
        SysctlPrinterVisitor visitor(m_opts, true);
        m_tree.GetRoot()->Accept(visitor);
        return success && visitor.Succeeded() ? 0 : 1;
    }

    // Process batch file if specified (-f file)
    if (!m_opts.inputFile.empty()) {
        std::ifstream infile(m_opts.inputFile);
        if (!infile.is_open()) {
            std::cerr << "sysctl: cannot open file: " << m_opts.inputFile << "\n";
            return 1;
        }
        std::string line;
        while (std::getline(infile, line)) {
            const auto comment = line.find('#');
            if (comment != std::string::npos) line.erase(comment);
            line = Trim(line);
            if (!line.empty()) {
                if (!ProcessLine(line)) success = false;
            }
        }
        if (infile.bad()) {
            std::cerr << "sysctl: failed to read file: " << m_opts.inputFile << "\n";
            success = false;
        }
    }

    // Process CLI queries
    for (const auto& query : m_queries) {
        if (!ProcessLine(query)) success = false;
    }

    return success ? 0 : 1;
}
