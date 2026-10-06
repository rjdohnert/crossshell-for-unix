#ifndef NFSCTL_CONTROLLERS_HPP
#define NFSCTL_CONTROLLERS_HPP

#include "nfsctl.hpp"
#include "reporter.hpp"
#include "engine.hpp"

class MountsToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options);
};

class NfsStatToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const GlobalOptions& options);
};

class ExportFsToolController {
public:
    static void PrintHelp();
    static DWORD ToolExportFs(bool suppressOutput = false, std::wstring* jsonPayload = nullptr);
    static DWORD Execute(const GlobalOptions& options);
};

class ShowMountToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const wchar_t* remote_host, const GlobalOptions& options);
};

class NfsConfToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options);
};

class NfsIdMapToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const wchar_t* account_name, const GlobalOptions& options);
};

class RpcDebugToolController {
public:
    static void PrintHelp();
    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options);
};

#endif // NFSCTL_CONTROLLERS_HPP
