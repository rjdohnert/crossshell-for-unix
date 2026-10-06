#include "unxz_backend_config.hpp"
#include "unxz_options.hpp"

void UnxzOptions::printHelp() {
        std::wcout << LR"(unxz(1)                 CrossShell for UNIX Reference Manual                 unxz(1)

    NAME
        unxz - decompress .xz files

    SYNOPSIS
        unxz [OPTIONS] [FILE...]

    DESCRIPTION
        Decompress FILEs in the .xz format.
        This frontend transparently delegates execution to an installed backend
        such as native xz (xz -d) or 7-Zip (7z x).
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -k, --keep
            Keep (do not delete) input files.

        -f, --force
            Force overwrite of output files.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        unxz archive.tar.xz
            Decompress archive.tar.xz to archive.tar.

        unxz -k data.xz
            Decompress data.xz while keeping the original .xz file.

    CrossShell for UNIX                                                      unxz(1)
)";
    }

void UnxzOptions::printVersion(const UnxzBackendConfig& backend) {
        std::wcout << L"unxz 1.0.0\n";
        if (!backend.applicationPath.empty()) {
            std::wcout << L"Active backend: " << backend.applicationPath << L"\n";
        } else {
            std::wcout << L"No backend detected in PATH or standard installation folders.\n";
        }
    }

bool UnxzOptions::parse(int argc, wchar_t* argv[], UnxzOptions& opts, bool& showHelp, bool& showVersion) {
        showHelp = false;
        showVersion = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i] ? argv[i] : L"";
            if (a == L"--help" || a == L"-h" || a == L"/?" || a == L"-?") {
                showHelp = true;
                return true;
            }
            if (a == L"--version" || a == L"-V" || a == L"-v") {
                showVersion = true;
                return true;
            }
            opts.forwardedArgs.push_back(a);
        }
        return true;
    }
