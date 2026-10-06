#include "mount_app.hpp"

int MountApplication::Run(int argc, wchar_t* argv[]) {
    MountOptions opts;
    if (!m_parser.Parse(argc, argv, opts)) {
        return 1;
    }

    if (opts.showHelp) {
        OptionParser::ShowUsage();
        return 0;
    }

    if (opts.showVersion) {
        OptionParser::ShowVersion();
        return 0;
    }

    if (!AdminValidator::IsRunningAsAdmin()) {
        std::wcerr << L"mount: this command requires administrator privileges. Run from an elevated terminal.\n";
        return 1;
    }

    // 1. List Mode
    if (opts.positionalArgs.empty() && !opts.unmountMode) {
        MountLister::ListMounts(opts.verbose);
        return 0;
    }

    // 2. Unmount Mode
    if (opts.unmountMode) {
        if (opts.positionalArgs.empty()) {
            std::wcerr << L"mount: missing target to unmount.\n";
            return 1;
        }
        return MountEngine::Unmount(opts.positionalArgs[0]) ? 0 : 1;
    }

    // 3. Mount Mode
    if (opts.positionalArgs.size() < 2) {
        std::wcerr << L"mount: missing source device or target mount point.\n";
        OptionParser::ShowUsage();
        return 1;
    }

    std::wstring source = StringUtils::TrimQuotes(opts.positionalArgs[0]);
    std::wstring target = StringUtils::TrimQuotes(opts.positionalArgs[1]);

    if (opts.fsType == L"smbfs" || opts.fsType == L"cifs" || source.rfind(L"\\\\", 0) == 0 || source.rfind(L"//", 0) == 0) {
        std::replace(source.begin(), source.end(), L'/', L'\\');
        return MountEngine::MountNetwork(source, target, opts.username, opts.password) ? 0 : 1;
    }

    if (source.rfind(L"\\\\?\\Volume", 0) == 0) {
        return MountEngine::MountVolume(source, target) ? 0 : 1;
    }

    std::wcerr << L"mount: unsupported device or filesystem type specified.\n";
    return 1;
}
