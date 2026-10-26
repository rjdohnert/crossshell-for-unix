#include "mib_tree.hpp"
#include <sstream>
#include <stdexcept>

// ============================================================================
// SysctlLeafNode implementation
// ============================================================================

SysctlLeafNode::SysctlLeafNode(std::string name, std::string path, std::string description,
                               GetterFunc getter, SetterFunc setter, Kind kind)
    : m_name(std::move(name)), m_path(std::move(path)),
      m_description(std::move(description)), m_getter(std::move(getter)),
      m_setter(std::move(setter)), m_kind(kind) {}

const std::string& SysctlLeafNode::GetName() const noexcept { return m_name; }
const std::string& SysctlLeafNode::GetPath() const noexcept { return m_path; }
bool SysctlLeafNode::IsDirectory() const noexcept { return false; }
void SysctlLeafNode::Accept(ISysctlVisitor& visitor) const { visitor.VisitLeaf(*this); }

const std::string& SysctlLeafNode::GetDescription() const noexcept { return m_description; }
std::string SysctlLeafNode::GetValue() const { return m_getter(); }
bool SysctlLeafNode::SetValue(const std::string& val) {
    if (!m_setter) return false;
    return m_setter(val);
}
bool SysctlLeafNode::IsWritable() const noexcept { return m_setter != nullptr; }
SysctlLeafNode::Kind SysctlLeafNode::GetKind() const noexcept { return m_kind; }

// ============================================================================
// SysctlDirectoryNode implementation
// ============================================================================

SysctlDirectoryNode::SysctlDirectoryNode(std::string name, std::string path)
    : m_name(std::move(name)), m_path(std::move(path)) {}

const std::string& SysctlDirectoryNode::GetName() const noexcept { return m_name; }
const std::string& SysctlDirectoryNode::GetPath() const noexcept { return m_path; }
bool SysctlDirectoryNode::IsDirectory() const noexcept { return true; }
void SysctlDirectoryNode::Accept(ISysctlVisitor& visitor) const { visitor.VisitDirectory(*this); }

void SysctlDirectoryNode::AddChild(std::shared_ptr<ISysctlNode> child) {
    m_children[child->GetName()] = std::move(child);
}

std::shared_ptr<ISysctlNode> SysctlDirectoryNode::GetChild(const std::string& name) const {
    auto it = m_children.find(name);
    return (it != m_children.end()) ? it->second : nullptr;
}

const std::map<std::string, std::shared_ptr<ISysctlNode>>& SysctlDirectoryNode::GetChildren() const noexcept {
    return m_children;
}

// ============================================================================
// MibTree implementation
// ============================================================================

MibTree::MibTree() : m_root(std::make_shared<SysctlDirectoryNode>("", "")) {}

void MibTree::RegisterNode(const std::string& path, const std::string& desc,
                           SysctlLeafNode::GetterFunc getter,
                           SysctlLeafNode::SetterFunc setter,
                           SysctlLeafNode::Kind kind) {
    if (path.empty() || path.front() == '.' || path.back() == '.' || path.find("..") != std::string::npos) {
        throw std::invalid_argument("Invalid MIB path: " + path);
    }

    std::stringstream ss(path);
    std::string segment;
    std::vector<std::string> segments;
    while (std::getline(ss, segment, '.')) {
        if (segment.empty()) throw std::invalid_argument("Empty path component in: " + path);
        segments.push_back(segment);
    }

    if (segments.empty()) throw std::invalid_argument("Empty path: " + path);

    std::shared_ptr<SysctlDirectoryNode> current = m_root;
    std::string currentPath = "";

    for (size_t i = 0; i < segments.size() - 1; ++i) {
        if (!currentPath.empty()) currentPath += ".";
        currentPath += segments[i];

        auto child = current->GetChild(segments[i]);
        if (!child) {
            auto newDir = std::make_shared<SysctlDirectoryNode>(segments[i], currentPath);
            current->AddChild(newDir);
            current = newDir;
        } else if (child->IsDirectory()) {
            current = std::static_pointer_cast<SysctlDirectoryNode>(child);
        } else {
            throw std::runtime_error("Path component is not a directory: " + segments[i]);
        }
    }

    const std::string& leafName = segments.back();
    if (current->GetChild(leafName)) {
        throw std::runtime_error("Node already exists: " + path);
    }
    auto leaf = std::make_shared<SysctlLeafNode>(leafName, path, desc, std::move(getter), std::move(setter), kind);
    current->AddChild(leaf);
}

std::shared_ptr<ISysctlNode> MibTree::FindNode(const std::string& path) const {
    if (path.empty()) return m_root;
    if (path.front() == '.' || path.back() == '.' || path.find("..") != std::string::npos) {
        return nullptr;
    }

    std::stringstream ss(path);
    std::string segment;
    std::shared_ptr<ISysctlNode> current = m_root;

    while (std::getline(ss, segment, '.')) {
        if (segment.empty()) return nullptr;
        if (!current->IsDirectory()) return nullptr;

        auto dir = std::static_pointer_cast<SysctlDirectoryNode>(current);
        current = dir->GetChild(segment);
        if (!current) return nullptr;
    }
    return current;
}

std::shared_ptr<SysctlDirectoryNode> MibTree::GetRoot() const noexcept {
    return m_root;
}
