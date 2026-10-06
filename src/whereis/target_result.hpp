#pragma once

#include "whereis.hpp"

struct TargetResult {
    std::wstring target;
    std::vector<std::wstring> bins;
    std::vector<std::wstring> mans;
    std::vector<std::wstring> srcs;
};
