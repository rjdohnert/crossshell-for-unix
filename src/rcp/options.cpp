#include "options.hpp"

RemotePath RemotePath::Parse(const std::string& input) {
    RemotePath rp;
    size_t colon_pos = input.find(':');

    if (colon_pos != std::string::npos) {
        if (colon_pos == 1 && std::isalpha(static_cast<unsigned char>(input[0]))) {
            rp.is_remote = false;
            rp.path = input;
            return rp;
        }

        rp.is_remote = true;
        std::string host_part = input.substr(0, colon_pos);
        rp.path = input.substr(colon_pos + 1);

        size_t at_pos = host_part.find('@');
        if (at_pos != std::string::npos) {
            rp.user = host_part.substr(0, at_pos);
            rp.host = host_part.substr(at_pos + 1);
        } else {
            rp.user = StringUtils::GetCurrentLocalUser();
            rp.host = host_part;
        }
    } else {
        rp.is_remote = false;
        rp.path = input;
    }
    return rp;
}

bool RcpOptions::Parse(const std::vector<std::string>& args) {
    std::vector<std::string> positional;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string arg = args[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            showVersion = true;
            return true;
        } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
            recursive = true;
        } else if (arg == "-p" || arg == "--preserve") {
            preserve = true;
        } else if (arg == "-P" && i + 1 < args.size()) {
            port = args[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            positional.push_back(arg);
        } else {
            std::cerr << "[ERROR] Unknown option: " << arg << "\n";
            return false;
        }
    }

    if (positional.size() < 2) {
        std::cerr << "[ERROR] Missing source or destination operand.\n";
        return false;
    }

    src = positional[0];
    dst = positional[1];
    return true;
}

void RcpOptions::PrintHelp(const std::string& exe_name) const {
    std::cout << R"(rcp(1)                  CrossShell for UNIX Reference Manual                   rcp(1)

    NAME
        rcp - remote file copy utility over SSH / network transport

    SYNOPSIS
        rcp [OPTIONS] [[USER@]HOST1:]FILE1 ... [[USER@]HOST2:]FILE2

    DESCRIPTION
        rcp copies files between machines across network endpoints or local
        filesystems securely.

    OPTIONS
        -r, -R, --recursive
            Recursively copy entire directory hierarchies.

        -p, --preserve
            Preserve modification times, access times, and file modes.

        -P PORT
            Connect to specified remote port.

        --json, --csv, --table
            Format file transfer manifest as JSON, CSV, or table.

        --pipe COMMAND
            Stream transfer progress to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        rcp -r src/ user@remote.server.com:/backup/src/
            Recursively copy directory tree to remote host.

    CrossShell for UNIX                                                    rcp(1)
)";
}

void RcpOptions::PrintVersion() const {
    std::cout << "rcp 3.0.0\n";
}
