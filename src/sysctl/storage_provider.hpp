#pragma once

#include "sysctl.hpp"
#include "mib_tree.hpp"

class DeviceHandle {
public:
    explicit DeviceHandle(const std::wstring& path);
    ~DeviceHandle();

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;

    HANDLE Get() const noexcept;
    bool IsValid() const noexcept;

private:
    HANDLE m_handle;
};

class WindowsStorageFactory {
public:
    static std::string DescriptorString(const std::vector<BYTE>& buffer, DWORD offset);
    static std::vector<std::string> EnumerateDisks();
    static std::vector<char> EnumerateVolumes();
    static std::wstring VolumePath(char letter);
    static std::string VolumeSpace(char letter, const std::string& field);
    static std::string VolumeInfo(char letter, bool wantLabel);
    static void Populate(MibTree& tree);
};
