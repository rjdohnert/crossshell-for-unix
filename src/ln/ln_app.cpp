#include "ln_app.hpp"
#include "engine.hpp"
#include <iostream>
#include <cwchar>

int LnApplication::Run(int argc, wchar_t* argv[]) {
    PrivilegeManager::EnableSymlinkPrivilege();

    LnOptions opts;
    if (!m_parser.Parse(argc, argv, opts)) {
        return 1;
    }

    if (opts.show_help) {
        OptionParser::PrintUsage();
        return 0;
    }

    if (opts.show_version) {
        OptionParser::PrintVersion();
        return 0;
    }

    if (opts.files.empty()) {
        std::fwprintf(stderr, L"ln: missing file operand\nTry 'ln --help' for more information.\n");
        return 1;
    }

    if (opts.files.size() == 1) {
        std::fwprintf(stderr, L"ln: missing destination file operand after '%ls'\n", opts.files[0].c_str());
        return 1;
    }

    LinkEngine engine(opts);
    bool success = true;

    if (opts.files.size() == 2) {
        fs::path source = opts.files[0];
        fs::path target = opts.files[1];

        if (PathInspector::IsDirectoryPath(target, opts.no_deref)) {
            target = target / source.filename();
        }

        if (!engine.CreateOneLink(source, target)) {
            success = false;
        }
    } else {
        fs::path target_dir = opts.files.back();

        if (!PathInspector::IsDirectoryPath(target_dir, opts.no_deref)) {
            std::fwprintf(stderr, L"ln: target '%ls' is not a directory\n", target_dir.c_str());
            return 1;
        }

        for (size_t i = 0; i < opts.files.size() - 1; ++i) {
            fs::path source = opts.files[i];
            fs::path target = target_dir / source.filename();

            if (!engine.CreateOneLink(source, target)) {
                success = false;
            }
        }
    }

    return success ? 0 : 1;
}
