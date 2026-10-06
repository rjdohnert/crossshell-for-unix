#ifndef MOUNT_ENGINE_HPP
#define MOUNT_ENGINE_HPP

#include "mount.hpp"

class MountLister {
public:
    static void ListMounts(bool verbose);
};

class MountEngine {
public:
    static bool Unmount(std::wstring target);
    static bool MountNetwork(const std::wstring& remote, const std::wstring& target, const std::wstring& user, const std::wstring& pass);
    static bool MountVolume(std::wstring volumeGuid, std::wstring targetFolder);
};

#endif // MOUNT_ENGINE_HPP
