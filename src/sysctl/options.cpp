#include "options.hpp"
#include <iostream>

void CliOptions::PrintHelp() const {
    std::cout <<
        "usage: sysctl [-AanNq] [-f file] [name[=value] ...]\n"
        "       sysctl [-deiw] (compatibility extensions)\n\n"
        "OpenBSD-style System Control Facility for Windows NT (not full kernel MIB parity)\n\n"
        "Options:\n"
        "  -a          List available values (default when no names or file are given).\n"
        "  -A          Also show table-query guidance and unavailable compatibility names.\n"
        "  -d          Print descriptions (extension).\n"
        "  -e          Separate the name and value with '=' (default; extension).\n"
        "  -f file     Read variable names or name=value pairs from the specified file.\n"
        "  -h, --help  Show this comprehensive help manual.\n"
        "  -i          Ignore unknown variable names (extension).\n"
        "  -n          Suppress printing of the variable name; output value only.\n"
        "  -N          Suppress printing of the variable value; output name only (extension).\n"
        "  -q          Suppress non-error notifications when setting variables.\n"
        "  -w          Accepted as a no-op; name=value implies a write.\n\n"
        "Only kern.hostname is writable; Windows requires privileges and a restart.\n\n"
        "Available Top-Level Namespaces:\n"
        "  kern.*      Operating system release, hostname, boot time, and kernel attributes.\n"
        "  hw.*        CPUs, memory, diskcount, disknames, and disk.PhysicalDriveN.*.\n"
        "  fs.*        Local drive-letter volumes: fs.volume.C.size/free/available, etc.\n"
        "  vm.*        Memory load, actual pagefile sizes, and commit limits.\n"
        "  net.*       IP forwarding and network stack counters.\n\n"
        "Examples:\n"
        "  sysctl -a\n"
        "  sysctl hw.ncpu hw.physmem\n"
        "  sysctl hw.*\n"
        "  sysctl -N kern\n"
        "  sysctl -n kern.hostname\n"
        "  sysctl hw.diskcount hw.disknames\n"
        "  sysctl hw.disk.PhysicalDrive0\n"
        "  sysctl fs.volume.C\n"
        "  sysctl -d kern.boottime\n"
        "  sysctl -w kern.hostname=WORKSTATION01\n";
}

bool CliOptions::Parse(int argc, char* argv[], std::vector<std::string>& queries) {
    bool parseOptions = true;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (parseOptions && (arg == "-h" || arg == "--help" || arg == "/?")) {
            help = true;
            return true;
        }

        if (parseOptions && arg == "--") {
            parseOptions = false;
            continue;
        }
        if (parseOptions && arg.length() > 1 && arg[0] == '-') {
            for (size_t c = 1; c < arg.length(); ++c) {
                switch (arg[c]) {
                    case 'a': listAll = true; break;
                    case 'A': listAll = true; listTables = true; break;
                    case 'd': showDesc = true; break;
                    case 'e': break;
                    case 'n': valueOnly = true; break;
                    case 'N': nameOnly = true; break;
                    case 'q': quiet = true; break;
                    case 'w': break;
                    case 'i': ignoreUnknown = true; break;
                    case 'f':
                        if (c + 1 < arg.length()) {
                            inputFile = arg.substr(c + 1);
                        } else if (i + 1 < argc) {
                            inputFile = argv[++i];
                        } else {
                            std::cerr << "sysctl: option requires an argument -- f\n";
                            return false;
                        }
                        if (inputFile.empty()) {
                            std::cerr << "sysctl: empty input file name\n";
                            return false;
                        }
                        c = arg.length() - 1;
                        break;
                    default:
                        std::cerr << "sysctl: unknown option -- " << arg[c] << "\n";
                        PrintHelp();
                        return false;
                }
            }
        } else {
            parseOptions = false;
            queries.push_back(arg);
        }
    }
    return true;
}
