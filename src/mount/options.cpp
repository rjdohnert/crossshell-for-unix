#include "options.hpp"

void OptionParser::ShowUsage() {
    std::wcout << LR"HELP(mount(1)                 CrossShell for UNIX Reference Manual                    mount(1)

    NAME
        mount - list, mount, and unmount Windows file systems

    SYNOPSIS
        mount
        mount -v
        mount -u TARGET
        mount -t smbfs [-o OPTIONS] //server/share DRIVE:
        mount VOLUME_GUID FOLDER

    DESCRIPTION
        Lists mounted file systems or mounts and unmounts Windows volumes, UNC
        shares, SMB/CIFS paths, and volume GUIDs.

    OPTIONS
        -t, --types TYPE       Select smbfs, cifs, or ntfs handling.
        -o, --options OPTIONS  Comma-separated user/password mount options.
        -u, -d, --umount       Unmount TARGET.
        -v, --verbose          Include mounted subfolder details.
        -h, /?, --help         Display this comprehensive reference manual.
        --version              Display version information and exit.
        --                     End options.

    EXAMPLES
        mount
        mount -v
        mount -u E:
        mount -t smbfs -o user=admin,pass=secret //server/share E:
        mount \\?\Volume{GUID} C:\Mount\Data

    EXIT STATUS
        0          Help, version, listing, or successful mount/unmount.
        1          Parse, privilege, unsupported-device, or Windows operation failure.

    CrossShell for UNIX                                                        mount(1)
)HELP";
}

void OptionParser::ShowVersion() {
    std::wcout << L"mount 1.0\n";
}

bool OptionParser::Parse(int argc, wchar_t* argv[], MountOptions& opts) const {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                opts.positionalArgs.push_back(argv[j]);
            }
            break;
        } else if (arg == L"-h" || arg == L"/?" || arg == L"--help") {
            opts.showHelp = true;
            return true;
        } else if (arg == L"--version") {
            opts.showVersion = true;
            return true;
        } else if (arg == L"-v" || arg == L"--verbose") {
            opts.verbose = true;
        } else if (arg == L"-u" || arg == L"-d" || arg == L"--umount") {
            opts.unmountMode = true;
        } else if ((arg == L"-t" || arg == L"--types") && i + 1 < argc) {
            opts.fsType = argv[++i];
        } else if ((arg == L"-o" || arg == L"--options") && i + 1 < argc) {
            opts.options = argv[++i];

            std::wstringstream ss(opts.options);
            std::wstring token;
            while (std::getline(ss, token, L',')) {
                std::wstring trimmed = token;
                auto is_space = [](wchar_t ch) { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; };
                trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [&](wchar_t ch) { return !is_space(ch); }));
                trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [&](wchar_t ch) { return !is_space(ch); }).base(), trimmed.end());
                if (trimmed.empty()) continue;
                size_t eq = trimmed.find(L'=');
                if (eq != std::wstring::npos) {
                    std::wstring key = trimmed.substr(0, eq);
                    std::wstring val = trimmed.substr(eq + 1);
                    if (key == L"user" || key == L"username") opts.username = val;
                    if (key == L"pass" || key == L"password") opts.password = val;
                }
            }
        } else {
            opts.positionalArgs.push_back(arg);
        }
    }
    return true;
}
