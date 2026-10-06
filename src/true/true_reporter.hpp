#pragma once

#include "true_options.hpp"
#include "true.hpp"

class TrueReporter {
public:
    static std::string wideToUtf8(const std::wstring& text);

    static int report(const TrueOptions& opts);
};
