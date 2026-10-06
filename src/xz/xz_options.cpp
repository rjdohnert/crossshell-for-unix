#include "backend_config.hpp"
#include "xz_options.hpp"

void XzOptions::printHelp() {
        std::wcout << LR"(xz(1)                     CrossShell for UNIX Reference Manual                  xz(1)

    NAME
        xz - compress or decompress .xz and .lzma files

    SYNOPSIS
        xz [OPTIONS] [FILE...]

    DESCRIPTION
        Compress or decompress FILEs in the .xz format.
        Transparently delegates execution to an installed backend (native xz
        or 7-Zip).
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -z, --compress
            Force compression.

        -d, --decompress, --uncompress
            Force decompression.

        -k, --keep
            Keep (do not delete) input files.

        -f, --force
            Force overwrite of output files and compress links.

        -h, --help, /?, -?
            Display this help and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        xz file.txt
            Compress file.txt into file.txt.xz and remove file.txt.

        xz -d file.txt.xz
            Decompress file.txt.xz into file.txt.

        xz -k archive.tar
            Compress archive.tar keeping the original file.

    CrossShell for UNIX                                                    xz(1)
)";
    }

void XzOptions::printVersion(const BackendConfig& backend) {
        std::wcout << L"xz (CrossShell) 1.0.0\n";
        if (!backend.applicationPath.empty()) {
            std::wcout << L"Active backend: " << backend.applicationPath << L"\n";
        } else {
            std::wcout << L"No backend detected in PATH or standard installation folders.\n";
        }
    }

bool XzOptions::parse(int argc, wchar_t* argv[], XzOptions& opts, bool& showHelp, bool& showVersion) {
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
