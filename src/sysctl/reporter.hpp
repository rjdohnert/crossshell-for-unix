#pragma once

#include "mib_tree.hpp"
#include "options.hpp"

class SysctlPrinterVisitor : public ISysctlVisitor {
public:
    explicit SysctlPrinterVisitor(const CliOptions& opts, bool listing = false);
    bool Succeeded() const noexcept;

    void VisitDirectory(const SysctlDirectoryNode& node) override;
    void VisitLeaf(const SysctlLeafNode& node) override;

private:
    const CliOptions& m_opts;
    bool m_listing;
    bool m_success = true;
};
