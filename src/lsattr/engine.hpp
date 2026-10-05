#pragma once

#include "lsattr.hpp"
#include <iostream>
#include <string>
#include <vector>

class ScopedFindHandle {
public:
    explicit ScopedFindHandle(HANDLE handle = INVALID_HANDLE_VALUE);
    ~ScopedFindHandle();

    ScopedFindHandle(const ScopedFindHandle&) = delete;
    ScopedFindHandle& operator=(const ScopedFindHandle&) = delete;

    ScopedFindHandle(ScopedFindHandle&& other) noexcept;
    ScopedFindHandle& operator=(ScopedFindHandle&& other) noexcept;

    HANDLE Get() const;
    bool IsValid() const;
    void Close();

private:
    HANDLE m_handle;
};

class AttributeInspector {
public:
    static std::wstring GetFlags(DWORD attrs);
    static std::wstring GetFlagsForPath(const std::wstring& path);
};

class LsattrTraverser {
public:
    static bool Collect(const std::wstring& path, bool recursive, std::vector<AttrRow>& rows, std::wostream& err);
};
