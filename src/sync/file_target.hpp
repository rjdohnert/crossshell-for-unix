#pragma once

#include "sync_target.hpp"
#include "sync.hpp"

class FileTarget : public ISyncTarget {
private:
    std::string path_str;
    std::wstring path_wstr;
    bool sync_containing_filesystem;

public:
    FileTarget(std::string path, bool file_system);

    std::string get_name() const override;

    std::string get_description() const override;

    bool flush(std::string& error_msg) override;
};
