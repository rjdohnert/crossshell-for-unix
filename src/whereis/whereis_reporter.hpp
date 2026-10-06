#pragma once

#include "target_result.hpp"
#include "whereis.hpp"

class WhereisReporter {
public:
    static std::string toUtf8(const std::wstring& text);

    static int dispatch(const std::vector<TargetResult>& results, int format, const std::wstring& pipeCommand);
};
