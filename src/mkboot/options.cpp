#include "options.hpp"

void PrintHelp(const char* exeName) {
    std::cout << R"(
===============================================================================
  mkboot v16.7.01 (WinPE Engine & ISO Builder)
===============================================================================

DESCRIPTION:
    Creates and configures bootable WinPE image files and bootable ISO images.
    This utility is inspired by HP-UX mkboot CLI semantics while applying native 
    Windows DISM servicing and IMAPI2 El Torito bootable filesystem structures.

USAGE:
    )" << (exeName ? exeName : "mkboot") << R"( -s <SourceDir|WimFile> -o <Output.iso> [OPTIONS]

REQUIRED PARAMETERS:
    -s, --source <path>         Path to WinPE source tree or base boot.wim file.
    -o, --output <path>         Path for the generated bootable .iso image file.

OPTIONS:
    -b, --boot-file <path>      Custom El Torito boot file (e.g., etfsboot.com or
                                efisys.bin). Default auto-detects from source.
    -d, --driver <path>         Inject driver (.inf file or folder) into WinPE.
                                Can be specified multiple times.
    --ignore-driver-errors      Proceed with commit even if a driver fails injection.
    -e, --exec <command>        Inject startup auto-execute command into WinPE 
                                (equivalent to HP-UX mkboot -a parameter).
                                Appends execution string to startnet.cmd.
    -a, --arch <arch>           Target Architecture: x64, x86, or arm64. Default: x64
    -l, --label <label>         Volume label for created ISO (default: WINPE_BOOT).
    -m, --mount-dir <path>      Explicit directory to mount WIM during modification.
    -f, --force                 Force overwriting existing destination files.
    -v, --verbose               Enable verbose output and detailed DISM/IMAPI logging.
    -h, --help                  Display this comprehensive help manual.

EXAMPLES:
    1. Create basic bootable WinPE ISO from folder source:
       mkboot -s C:\WinPE_amd64\media -o C:\Images\bootable.iso

    2. Service WIM with drivers and startup commands (HP-UX -a equivalent):
       mkboot -s C:\WinPE_amd64\media -o C:\Images\custom_boot.iso \
              -d C:\Drivers\Network -e "netuse Z: \\server\share /user:admin pass" \
              -e "Z:\setup.exe"

    3. Service WIM and ignore non-critical driver injection warnings:
       mkboot -s C:\WinPE_amd64\media -o C:\Images\custom_boot.iso \
              -d C:\Drivers\Experimental --ignore-driver-errors

    4. Overwrite output and specify explicit bootloader:
       mkboot -s C:\WinPE_amd64 -o C:\boot.iso -b C:\WinPE_amd64\Boot\etfsboot.com --force
)" << std::endl;
}

bool ParseCommandLine(int argc, char* argv[], ConfigOptions& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            config.showHelp = true;
            return true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
            Logger::SetVerbose(true);
        } else if (arg == "-f" || arg == "--force") {
            config.force = true;
        } else if (arg == "--ignore-driver-errors") {
            config.ignoreDriverErrors = true;
        } else if (arg == "-s" || arg == "--source") {
            if (i + 1 < argc) config.sourcePath = argv[++i];
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) config.outputIsoPath = argv[++i];
        } else if (arg == "-b" || arg == "--boot-file") {
            if (i + 1 < argc) config.bootFilePath = argv[++i];
        } else if (arg == "-d" || arg == "--driver") {
            if (i + 1 < argc) config.driverPaths.emplace_back(argv[++i]);
        } else if (arg == "-e" || arg == "--exec" || arg == "-a" || arg == "--auto-exec") {
            if (i + 1 < argc) config.execCommands.emplace_back(argv[++i]);
        } else if (arg == "-l" || arg == "--label") {
            if (i + 1 < argc) config.volumeLabel = argv[++i];
        } else if (arg == "-m" || arg == "--mount-dir") {
            if (i + 1 < argc) config.mountPath = argv[++i];
        } else if (arg == "--arch") {
            if (i + 1 < argc) config.architecture = argv[++i];
        } else {
            Logger::Log(LogLevel::Error, "Unknown command line option: " + arg);
            return false;
        }
    }
    return true;
}
