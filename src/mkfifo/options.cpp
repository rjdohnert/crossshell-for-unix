#include "options.hpp"

void HelpFormatter::printVersion() {
    std::cout << "mkfifo version 1.0.0\n";
}

void HelpFormatter::printHelp() {
    std::cout << R"RAW(mkfifo(1)               CrossShell for UNIX Reference Manual           mkfifo(1)

NAME
     mkfifo - make FIFO special files

SYNOPSIS
     mkfifo [-m sddl_dacl] [-t byte|message] [-d duplex|in|out] [-p] [-v] name ...
     mkfifo [-h | --help] [-V | --version]

DESCRIPTION
     The mkfifo command creates FIFO special files (named pipes) in the order
     specified. On Windows NT architectures, named pipes reside within the 
     Named Pipe Filesystem (NPFS) under the namespace '\\.\pipe\name'.

     Unlike UNIX filesystems, Windows deletes NPFS endpoints as soon as all
     open handles are closed. To keep the FIFO alive for other terminal sessions
     or background processes, specify the -p (--persist) option.

WINDOWS MODES
     Standard UNIX octal permission masks (such as 0666 or 0644) are not 
     synthesized. Instead, authentic Windows security and IPC modes are 
     configured directly:

     Pipe Direction (-d):
          duplex    Bi-directional communication (PIPE_ACCESS_DUPLEX) [Default]
          in        Client-to-server inbound (PIPE_ACCESS_INBOUND)
          out       Server-to-client outbound (PIPE_ACCESS_OUTBOUND)

     Transmission Mode (-t):
          byte      Continuous raw byte stream (PIPE_TYPE_BYTE) [Default]
          message   Discrete message frames (PIPE_TYPE_MESSAGE)

     Security DACL (-m):
          Configured using genuine Windows Security Descriptor Definition 
          Language (SDDL). Default is 'D:(A;;GRGW;;;WD)' (Everyone: Read/Write).

OPTIONS
     -m mode   Specifies the Windows DACL in SDDL format.
               Common examples:
                 "D:(A;;GRGW;;;WD)"             - Everyone Read/Write [Default]
                 "D:(A;;GA;;;BA)"               - Built-in Administrators Full Access
                 "D:(A;;GRGW;;;AU)(A;;GA;;;BA)" - Auth Users RW, Admins Full

     -d dir    Sets pipe direction: duplex, in, or out.

     -t type   Sets transmission type: byte or message.

     -p        (Persist) Keeps server handle(s) open in NPFS until Ctrl+C is 
               issued, ensuring the FIFO is discoverable by other processes.

     -v        (Verbose) Prints underlying Win32 pipe attributes upon creation.

     -h, --help
               Display this reference manual and exit.

     -V, --version
               Display version information and exit.

EXAMPLES
     mkfifo my_pipe
     mkfifo -p -v /tmp/app_fifo
     mkfifo -t message -m "D:(A;;GA;;;BA)" \\.\pipe\secure_fifo
     mkfifo -d in -p logger_fifo

CrossShell for UNIX                                              mkfifo(1)
)RAW";
}

bool ArgumentParser::parse(int argc, char* argv[], FifoOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h" || arg == "/?") {
            opts.showHelp = true;
            return true;
        }
        if (arg == "--version" || arg == "-V") {
            opts.showVersion = true;
            return true;
        }
        if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "-p" || arg == "--persist") {
            opts.persist = true;
            continue;
        }
        if (arg == "-m" && i + 1 < argc) {
            opts.mode.sddlString = argv[++i];
            continue;
        }
        if (arg == "-t" && i + 1 < argc) {
            std::string t = argv[++i];
            if (t == "message") opts.mode.type = WindowsPipeMode::Type::Message;
            else if (t == "byte") opts.mode.type = WindowsPipeMode::Type::Byte;
            else {
                std::cerr << "mkfifo: invalid transmission type '" << t << "' (use byte or message)\n";
                return false;
            }
            continue;
        }
        if (arg == "-d" && i + 1 < argc) {
            std::string d = argv[++i];
            if (d == "in") opts.mode.direction = WindowsPipeMode::Direction::Inbound;
            else if (d == "out") opts.mode.direction = WindowsPipeMode::Direction::Outbound;
            else if (d == "duplex") opts.mode.direction = WindowsPipeMode::Direction::Duplex;
            else {
                std::cerr << "mkfifo: invalid direction '" << d << "' (use duplex, in, or out)\n";
                return false;
            }
            continue;
        }

        if (arg.rfind("-", 0) == 0) {
            std::cerr << "mkfifo: illegal option -- " << arg << "\n"
                      << "usage: mkfifo [-m sddl] [-t byte|message] [-d duplex|in|out] [-p] [-v] name ...\n";
            return false;
        }

        opts.pipeNames.push_back(arg);
    }

    if (opts.pipeNames.empty()) {
        std::cerr << "mkfifo: missing operand\nTry 'mkfifo --help' for more information.\n";
        return false;
    }

    return true;
}
