#pragma once

#include "whence_result.hpp"
#include "whence.hpp"

class WhenceReporter {
public:
    static std::string toUtf8(const std::wstring& text);

    static int dispatch(const std::vector<WhenceResult>& results, int format, bool verbose, const std::wstring& pipeCommand);
};
