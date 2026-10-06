#pragma once

#include "system_details.hpp"
#include "uname_options.hpp"
#include "uname.hpp"

class UnameReporter {
public:
    static std::string toUtf8(const std::wstring& text);

    static int dispatch(const SystemDetails& sys, const UnameOptions& opts);
};
