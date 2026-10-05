#pragma once

#include "lsvg.hpp"

class Win32Handle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    Win32Handle(HANDLE h);
    ~Win32Handle();
    bool isValid() const;
    operator HANDLE() const;
};

class StorageManager {
public:
    static std::string WideToUtf8(const std::wstring& value);
    static std::vector<VolumeGroup> DiscoverVolumeGroups();
};
