#ifndef PROCESS_LAUNCHER_HPP
#define PROCESS_LAUNCHER_HPP

#include "runcon.hpp"

class IntegrityProcessLauncher {
private:
    static std::wstring buildCommandLine(const std::vector<std::wstring>& args);

public:
    static int launch(const std::wstring& stringSid, const std::vector<std::wstring>& args);
};

#endif // PROCESS_LAUNCHER_HPP
