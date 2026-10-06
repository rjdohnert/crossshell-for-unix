#pragma once

#include "reboot_options.hpp"
#include "reboot.hpp"

class RebootEngine {
public:
    static std::string ToUpper(std::string str);

    static int Execute(const RebootOptions& options);
};
