#pragma once

#include "sysctl.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

class SysctlDirectoryNode;
class SysctlLeafNode;

class ISysctlVisitor {
public:
    virtual ~ISysctlVisitor() = default;
    virtual void VisitDirectory(const SysctlDirectoryNode& node) = 0;
    virtual void VisitLeaf(const SysctlLeafNode& node) = 0;
};

class ISysctlNode {
public:
    virtual ~ISysctlNode() = default;
    virtual const std::string& GetName() const noexcept = 0;
    virtual const std::string& GetPath() const noexcept = 0;
    virtual bool IsDirectory() const noexcept = 0;
    virtual void Accept(ISysctlVisitor& visitor) const = 0;
};

class SysctlLeafNode : public ISysctlNode {
public:
    enum class Kind { Value, Table, Unavailable };
    using GetterFunc = std::function<std::string()>;
    using SetterFunc = std::function<bool(const std::string&)>;

    SysctlLeafNode(std::string name, std::string path, std::string description,
                   GetterFunc getter, SetterFunc setter = nullptr,
                   Kind kind = Kind::Value);

    const std::string& GetName() const noexcept override;
    const std::string& GetPath() const noexcept override;
    bool IsDirectory() const noexcept override;
    void Accept(ISysctlVisitor& visitor) const override;

    const std::string& GetDescription() const noexcept;
    std::string GetValue() const;
    bool SetValue(const std::string& val);
    bool IsWritable() const noexcept;
    Kind GetKind() const noexcept;

private:
    std::string m_name;
    std::string m_path;
    std::string m_description;
    GetterFunc m_getter;
    SetterFunc m_setter;
    Kind m_kind;
};

class SysctlDirectoryNode : public ISysctlNode {
public:
    SysctlDirectoryNode(std::string name, std::string path);

    const std::string& GetName() const noexcept override;
    const std::string& GetPath() const noexcept override;
    bool IsDirectory() const noexcept override;
    void Accept(ISysctlVisitor& visitor) const override;

    void AddChild(std::shared_ptr<ISysctlNode> child);
    std::shared_ptr<ISysctlNode> GetChild(const std::string& name) const;
    const std::map<std::string, std::shared_ptr<ISysctlNode>>& GetChildren() const noexcept;

private:
    std::string m_name;
    std::string m_path;
    std::map<std::string, std::shared_ptr<ISysctlNode>> m_children;
};

class MibTree {
public:
    MibTree();

    void RegisterNode(const std::string& path, const std::string& desc,
                      SysctlLeafNode::GetterFunc getter,
                      SysctlLeafNode::SetterFunc setter = nullptr,
                      SysctlLeafNode::Kind kind = SysctlLeafNode::Kind::Value);

    std::shared_ptr<ISysctlNode> FindNode(const std::string& path) const;
    std::shared_ptr<SysctlDirectoryNode> GetRoot() const noexcept;

private:
    std::shared_ptr<SysctlDirectoryNode> m_root;
};
