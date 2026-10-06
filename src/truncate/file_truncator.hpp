#pragma once

#include "size_calculator.hpp"
#include "truncate.hpp"

class FileTruncator {
private:
    bool no_create = false;
    std::unique_ptr<SizeCalculator> size_calc;
    std::wstring ref_file;

public:
    void set_no_create(bool flag);
    void set_size_calculator(SizeCalculator calc);
    void set_reference_file(std::wstring path);

    bool truncate_file(const std::wstring& wpath, std::string& err_msg);

private:
    static bool get_file_size(const std::wstring& path, uint64_t& out_size, std::string& err);

    static std::string win32_error_to_string(DWORD err);
};
