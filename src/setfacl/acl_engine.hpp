#ifndef ACL_ENGINE_HPP
#define ACL_ENGINE_HPP

#include "setfacl.hpp"

class AclEngine {
public:
    static bool ApplyEntry(const std::wstring& path, const FaclEntry& entry);
    static bool RemoveDacl(const std::wstring& path);
    static bool ProcessDirectoryRecursive(const std::wstring& dirPath, bool removeDacl, const FaclEntry& entry);
};

#endif // ACL_ENGINE_HPP
