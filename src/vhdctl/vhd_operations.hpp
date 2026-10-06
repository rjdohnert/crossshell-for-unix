#pragma once

#include "vhdctl.hpp"

class VhdOperations {
public:
    static bool CreateDisk(const std::wstring& path, ULONGLONG sizeInBytes, bool isFixed, OutputFormat format);

    static bool MountDisk(const std::wstring& path, bool readOnly, OutputFormat format);

    static bool UnmountDisk(const std::wstring& path, OutputFormat format);
};
