#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "nohup.hpp"
#include "options.hpp"

class DetachedProcessLauncher {
public:
    static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
    static int Launch(int argc, wchar_t* argv[], const NohupOptions& options);
};

#endif // ENGINE_HPP
