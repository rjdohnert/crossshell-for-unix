#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pkg.hpp"
#include "options.hpp"

class LocalPackageInstaller {
public:
    static int installLocal(const std::wstring& packagePath);
};

class WingetBridge {
public:
    static int execute(const PkgOptions& options);
};

#endif // ENGINE_HPP
