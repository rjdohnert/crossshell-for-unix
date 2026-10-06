#include "temporary_file.hpp"

std::string make_temp_file(const std::string& prefix) {
    fs::path p = fs::temp_directory_path() /
        (prefix + "-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()) + ".txt");
    return p.string();
}
