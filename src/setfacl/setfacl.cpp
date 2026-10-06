/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "setfacl.hpp"
#include "principal_resolver.hpp"
#include "acl_engine.hpp"
#include "setfacl_options.hpp"

class SetfaclApplication {
private:
    SetfaclOptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"setfacl";

        SetfaclOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            if (opts.showHelp) {
                SetfaclOptionParser::PrintUsage(progName);
                return 0;
            }
            if (opts.showVersion) {
                SetfaclOptionParser::PrintVersion();
                return 0;
            }
            SetfaclOptionParser::PrintUsage(progName);
            return 1;
        }

        if (opts.showHelp) {
            SetfaclOptionParser::PrintUsage(progName);
            return 0;
        }

        if (opts.showVersion) {
            SetfaclOptionParser::PrintVersion();
            return 0;
        }

        FaclEntry entry{};
        if (!opts.removeDacl) {
            size_t first = opts.spec.find(L':');
            size_t second = opts.spec.find(L':', first == std::wstring::npos ? first : first + 1);
            if (first == std::wstring::npos || second == std::wstring::npos) {
                std::wcerr << L"setfacl: invalid ACL spec '" << opts.spec << L"'\n";
                return 1;
            }

            entry.kind = opts.spec.substr(0, first);
            entry.name = opts.spec.substr(first + 1, second - first - 1);
            entry.rights = opts.spec.substr(second + 1);
        }

        bool ok = true;
        for (const auto& path : opts.paths) {
            DWORD attrs = GetFileAttributesW(path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::wcerr << L"setfacl: " << path << L": No such file or directory\n";
                ok = false;
                continue;
            }

            if (opts.removeDacl) {
                if (!AclEngine::RemoveDacl(path)) ok = false;
            } else {
                if (!AclEngine::ApplyEntry(path, entry)) ok = false;
            }

            if (opts.recursive && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                if (!AclEngine::ProcessDirectoryRecursive(path, opts.removeDacl, entry)) {
                    ok = false;
                }
            }
        }

        return ok ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SetfaclApplication app;
    return app.Run(argc, argv);
}
