#pragma once

#include "umask.hpp"

class UmaskOptions {
public:
    bool flag_symbolic = false;
    bool flag_print_reusable = false;
    bool show_help = false;
    std::string new_mask_arg;
    std::wstring subcommand_str;

    bool Parse(int argc, char* argv[]);

    void PrintHelp() const;
};
