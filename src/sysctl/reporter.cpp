#include "reporter.hpp"
#include "sysctl.hpp"
#include <iostream>
#include <ctime>

using namespace sysctl;

SysctlPrinterVisitor::SysctlPrinterVisitor(const CliOptions& opts, bool listing)
    : m_opts(opts), m_listing(listing) {}

bool SysctlPrinterVisitor::Succeeded() const noexcept {
    return m_success;
}

void SysctlPrinterVisitor::VisitDirectory(const SysctlDirectoryNode& node) {
    for (const auto& child : node.GetChildren()) {
        child.second->Accept(*this);
    }
}

void SysctlPrinterVisitor::VisitLeaf(const SysctlLeafNode& node) {
    if (m_listing && !m_opts.listTables && node.GetKind() != SysctlLeafNode::Kind::Value) return;
    if (m_opts.showDesc) {
        std::cout << node.GetPath() << ": " << node.GetDescription() << "\n";
        return;
    }

    try {
        std::string value = node.GetValue();
        if (node.GetKind() != SysctlLeafNode::Kind::Value) {
            std::cerr << "sysctl: " << node.GetPath() << ": " << value << "\n";
            if (!m_listing && node.GetKind() == SysctlLeafNode::Kind::Unavailable) m_success = false;
            return;
        }
        if (node.GetPath() == "kern.boottime" && !m_opts.valueOnly) {
            const std::time_t timestamp = static_cast<std::time_t>(std::stoll(value));
            char date[26]{};
            const auto error = ctime_s(date, sizeof(date), &timestamp);
            if (error != 0) throw std::system_error(error, std::generic_category(), "ctime_s");
            value = Trim(date);
        }
        if (m_opts.nameOnly) {
            std::cout << node.GetPath() << "\n";
        } else if (m_opts.valueOnly) {
            std::cout << value << "\n";
        } else {
            std::cout << node.GetPath() << "=" << value << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "sysctl: " << node.GetPath() << ": " << ex.what() << "\n";
        m_success = false;
    }
}
