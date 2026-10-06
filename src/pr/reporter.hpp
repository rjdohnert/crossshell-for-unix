#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "pr.hpp"

class PrReporter {
public:
    static std::string toUtf8(const std::wstring& text);
    static int dispatch(const std::wstring& content, int format, const std::wstring& pipeCommand);
};

#endif // REPORTER_HPP
