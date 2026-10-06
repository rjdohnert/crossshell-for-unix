#ifndef VMS_STATUS_HPP
#define VMS_STATUS_HPP

#include "spawn.hpp"

class VmsStatusReporter {
private:
    int outputFormat{0};
    std::wstring pipeCommand;

public:
    VmsStatusReporter(int fmt, std::wstring pipeCmd);
    void printStatus(wchar_t severity, std::wstring_view facility, std::wstring_view ident, std::wstring_view text) const;
    static std::wstring toUpper(std::wstring_view str);
};

#endif // VMS_STATUS_HPP
