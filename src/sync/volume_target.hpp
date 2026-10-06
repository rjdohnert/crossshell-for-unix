#pragma once

#include "sync_target.hpp"
#include "sync.hpp"

class VolumeTarget : public ISyncTarget {
private:
    std::wstring root_path;     // e.g. L"C:\\"
    std::wstring device_path;   // e.g. L"\\\\.\\C:"
    std::string drive_letter;   // e.g. "C:"
    std::string fs_name;        // e.g. "NTFS"
    std::string volume_label;   // e.g. "System"
    UINT drive_type;            // DRIVE_FIXED, DRIVE_REMOVABLE, etc.

public:
    VolumeTarget(wchar_t letter, UINT type);

    std::string get_name() const override;

    std::string get_description() const override;

    UINT get_drive_type() const;

    bool flush(std::string& error_msg) override;
};

// Target representing a single file or directory
