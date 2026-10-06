#pragma once

#include "prtconf.hpp"
#include "options.hpp"
#include "reporter.hpp"

class SystemConfigScanner {
private:
    static std::string toUtf8(const std::wstring& w);
    static std::wstring getPropString(HDEVINFO info, SP_DEVINFO_DATA& dev, const DEVPROPKEY& key);
    static std::string archWord(WORD arch);

public:
    static SystemConfigSummary collect();
};

class PrtconfEngine {
private:
    PrtconfOptions options;

public:
    explicit PrtconfEngine(PrtconfOptions opts);
    int execute();
};
