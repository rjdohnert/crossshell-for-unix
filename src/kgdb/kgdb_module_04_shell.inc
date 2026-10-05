// --- Shell & Help Manager ---
class KgdbShell {
private:
    KgdbEngine engine;

public:
    static void PrintBanner() {
        std::cout << BOLD << CYAN;
        std::cout << "\n";
        std::cout << "                         kgdb v4.6.0 Kernel Debugger\n";
        std::cout << "  Type \"help\" for usage commands or \"help examples\" for step-by-step guides\n";
        std::cout << "   Targets: Microsoft Windows | Microsoft Azure | FreeBSD | Hyper-V | WSL2\n";
        std::cout << "\n";
        std::cout << RESET;
    }

    static void PrintCliHelp() {
           std::cout << R"HELP(kgdb(1)                 CrossShell for UNIX Reference Manual                    kgdb(1)

    NAME
        kgdb - connect to and inspect supported kernel debugging targets

    SYNOPSIS
        kgdb [OPTIONS] [DUMP_FILE]

    DESCRIPTION
        Connects to Windows, Azure, FreeBSD, WSL2, Hyper-V, or remote GDB/RSP
        targets. Without --exec, kgdb enters the interactive (kgdb) debugger.

    OPTIONS
        --local                 Attach to the live local Windows kernel.
        --remote-win HOST:PORT  Attach to a remote Windows kernel.
        --azure ENDPOINT        Attach to an Azure VM debugging endpoint.
        --bsd [TARGET]          Attach to FreeBSD or load a vmcore dump.
        --wsl [DISTRO]          Attach to a WSL2 kernel.
        --hyperv PIPE           Attach through a Hyper-V named pipe.
        --remote HOST:PORT      Attach to a remote GDB RSP stub.
        -d, --dump PATH         Load a Windows MEMORY.DMP or BSD vmcore.
        -e, --exec COMMANDS     Execute semicolon-separated commands and exit.
        --no-color              Disable colored output.
        -h, --help              Display this comprehensive reference manual.
        -v, --version           Display version information and exit.

    INTERACTIVE TOPICS
        At the (kgdb) prompt, use help, help examples, info registers, bt, and
        kldstat to inspect the current target.

    EXAMPLES
        kgdb --local -e "kldstat; info registers"
        kgdb --remote-win 192.168.1.100:50000 -e "info registers; bt"
        kgdb --azure 20.12.55.10:1234
        kgdb --bsd C:\CrashDumps\vmcore.0
        kgdb --wsl Ubuntu-22.04

    EXIT STATUS
        0          Help, version, successful --exec, or normal termination.
        1          Initialization, option, or endpoint failure.

    CrossShell for UNIX                                                        kgdb(1)
    )HELP";
           return;

        std::cout << BOLD << CYAN;
        std::cout << "\n";
        std::cout << "                          kgdb v4.6.0 Kernel Debugger\n";
        std::cout << "    Targets: Microsoft Windows | Microsoft Azure | FreeBSD | Hyper-V | WSL2\n";
        std::cout << "\n" << RESET;

        std::cout << BOLD << "USAGE:\n" << RESET;
        std::cout << "  kgdb [options]\n";
        std::cout << "  kgdb [options] <dump_file>\n\n";

        std::cout << BOLD << "TARGET CONNECTION OPTIONS:\n" << RESET;
        std::cout << "  " << GREEN << "--local" << RESET << "                      Attach to live Local Windows Kernel (via Win32 EnumDeviceDrivers)\n";
        std::cout << "  " << GREEN << "--remote-win <host:port>" << RESET << "     Attach to Remote Windows Kernel over KDNET / GDB RSP\n";
        std::cout << "  " << GREEN << "--azure <endpoint>" << RESET << "          Attach to Azure VM GDB/KDNET stub (e.g. 20.12.55.10:1234)\n";
        std::cout << "  " << GREEN << "--bsd [<ip:port> | <vmcore>]" << RESET << " Attach to FreeBSD kernel GDB stub or load vmcore dump\n";
        std::cout << "  " << GREEN << "--wsl [<distro>]" << RESET << "             Attach to WSL2 microVM Linux kernel\n";
        std::cout << "  " << GREEN << "--hyperv <pipe_name>" << RESET << "         Attach to Hyper-V VM via Named Pipe\n";
        std::cout << "  " << GREEN << "--remote <host:port>" << RESET << "         Attach to remote kernel GDB RSP stub over TCP\n";
        std::cout << "  " << GREEN << "--dump <path>" << RESET << "                Load Windows (MEMORY.DMP) or BSD (vmcore) crash dump\n\n";

        PrintHelpTopic("examples");
    }

    static void PrintHelpTopic(const std::string& topic) {
        if (topic == "local") {
            std::cout << BOLD << "LOCAL WINDOWS KERNEL INSPECTION SETUP:\n" << RESET;
            std::cout << "  Inspect live local Windows kernel drivers and base addresses:\n";
            std::cout << "     $ " << GREEN << "kgdb --local" << RESET << "\n";
            std::cout << "     (kgdb) " << GREEN << "kldstat" << RESET << "\n\n";
        } else if (topic == "remote-win") {
            std::cout << BOLD << "REMOTE WINDOWS KERNEL DEBUGGING SETUP (KDNET):\n" << RESET;
            std::cout << "  Connect to remote Windows system with KDNET / GDB enabled:\n";
            std::cout << "     $ " << GREEN << "kgdb --remote-win 192.168.1.100:50000" << RESET << "\n\n";
        } else if (topic == "azure") {
            std::cout << BOLD << "MICROSOFT AZURE VM KERNEL DEBUGGING SETUP:\n" << RESET;
            std::cout << "  Connect over Azure VM Network Endpoint / GDB Stub:\n";
            std::cout << "     $ " << GREEN << "kgdb --azure 20.12.55.10:1234" << RESET << "\n\n";
        } else if (topic == "bsd") {
            std::cout << BOLD << "FREEBSD / BSD KERNEL DEBUGGING SETUP:\n" << RESET;
            std::cout << "  1. Remote FreeBSD Kernel Debugging over TCP / Serial:\n";
            std::cout << "     $ " << GREEN << "kgdb --bsd 192.168.1.50:1234" << RESET << "\n";
            std::cout << "  2. Load FreeBSD Kernel Core Dump (vmcore.0):\n";
            std::cout << "     $ " << GREEN << "kgdb --bsd C:\\CrashDumps\\vmcore.0" << RESET << "\n\n";
        } else if (topic == "wsl") {
            std::cout << BOLD << "WSL2 LINUX KERNEL DEBUGGING SETUP:\n" << RESET;
            std::cout << "  1. Attach using win-kgdb:\n";
            std::cout << "     $ " << GREEN << "kgdb --wsl Ubuntu-22.04" << RESET << "\n\n";
        } else if (topic == "hyperv") {
            std::cout << BOLD << "HYPER-V VM NAMED PIPE DEBUGGING SETUP:\n" << RESET;
            std::cout << "  1. Configure Hyper-V COM1 Pipe in PowerShell:\n";
            std::cout << "     " << CYAN << "Set-VMComPort -VMName \"Win11_Dev\" -Number 1 -Path \"\\\\.\\pipe\\Win11_Pipe\"" << RESET << "\n";
            std::cout << "  2. Attach win-kgdb to Hyper-V pipe:\n";
            std::cout << "     $ " << GREEN << "kgdb --hyperv Win11_Pipe" << RESET << "\n\n";
        } else if (topic == "examples") {
            std::cout << BOLD << "REAL-WORLD WORKFLOW EXAMPLES:\n" << RESET;
            std::cout << "  " << YELLOW << "[Example 1: Local Windows Kernel Driver Table]" << RESET << "\n";
            std::cout << "    $ " << GREEN << "kgdb --local -e \"kldstat; info registers\"" << RESET << "\n\n";

            std::cout << "  " << YELLOW << "[Example 2: Remote Windows Kernel Debugging]" << RESET << "\n";
            std::cout << "    $ " << GREEN << "kgdb --remote-win 192.168.1.100:50000 -e \"info registers; bt\"" << RESET << "\n\n";

            std::cout << "  " << YELLOW << "[Example 3: Azure VM Kernel Debugging]" << RESET << "\n";
            std::cout << "    $ " << GREEN << "kgdb --azure 20.12.55.10:1234 -e \"info registers; bt; kldstat\"" << RESET << "\n\n";

            std::cout << "  " << YELLOW << "[Example 4: FreeBSD Kernel Debugging]" << RESET << "\n";
            std::cout << "    $ " << GREEN << "kgdb --bsd 192.168.1.50:1234 -e \"info registers; bt; kldstat\"" << RESET << "\n\n";
        } else {
            std::cout << "Topics: local, remote-win, azure, bsd, wsl, hyperv, remote, examples\n";
        }
    }

    void ExecuteCommand(const std::string& line) {
        if (line.empty()) return;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "help" || cmd == "h") {
            std::string topic; ss >> topic;
            PrintHelpTopic(topic);
        } else if (cmd == "info" || cmd == "i") {
            std::string subCmd; ss >> subCmd;
            if (subCmd == "registers" || subCmd == "r") engine.PrintRegisters();
            else if (subCmd == "modules" || subCmd == "m") engine.PrintModules();
        } else if (cmd == "bt" || cmd == "backtrace" || cmd == "where") {
            engine.PrintBacktrace();
        } else if (cmd == "kldstat" || cmd == "lsmod") {
            engine.PrintModules();
        } else if (cmd == "analyze" || cmd == "bugcheck") {
            engine.AnalyzeBugCheck();
        } else if (cmd == "target") {
            std::string sub; ss >> sub;
            if (sub == "local") {
                engine.AttachLocal();
            } else if (sub == "remote-win") {
                std::string target; ss >> target;
                engine.AttachRemoteWin(target);
            } else if (sub == "azure") {
                std::string target; ss >> target;
                engine.AttachAzure(target);
            } else if (sub == "bsd") {
                std::string target; ss >> target;
                engine.AttachBsd(target);
            } else if (sub == "wsl") {
                std::string distro; ss >> distro;
                engine.AttachWsl(distro);
            } else if (sub == "hyperv") {
                std::string pipeName; ss >> pipeName;
                engine.AttachHyperV(pipeName);
            } else if (sub == "remote") {
                std::string endpoint; ss >> endpoint;
                size_t colon = endpoint.find(':');
                if (colon != std::string::npos) {
                    engine.AttachRemoteTcp(endpoint.substr(0, colon), std::stoi(endpoint.substr(colon + 1)));
                }
            } else if (sub == "dump") {
                std::string path; ss >> path;
                engine.LoadDumpFile(path);
            }
        } else if (cmd.rfind("x/", 0) == 0) {
            std::string addrStr; ss >> addrStr;
            uint64_t addr = addrStr.empty() ? 0xfffff80087654321 : std::stoull(addrStr, nullptr, 16);
            engine.HexDumpMemory(addr, 32);
        } else if (cmd == "step" || cmd == "s") {
            engine.StepOrContinue("s");
        } else if (cmd == "continue" || cmd == "c") {
            engine.StepOrContinue("c");
        } else {
            std::cout << RED << "Undefined command: \"" << cmd << "\". Type \"help\"." << RESET << "\n";
        }
    }

    void ExecuteScript(const std::string& script) {
        std::stringstream ss(script);
        std::string token;
        while (std::getline(ss, token, ';')) {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                std::cout << GREEN << BOLD << "(kgdb) " << RESET << token << "\n";
                ExecuteCommand(token);
            }
        }
    }

    void RunInteractive() {
        PrintBanner();
        std::string line;
        while (true) {
            std::cout << GREEN << BOLD << "(kgdb) " << RESET;
            if (!std::getline(std::cin, line)) break;
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            if (line == "quit" || line == "q") break;
            ExecuteCommand(line);
        }
    }

    KgdbEngine& GetEngine() { return engine; }
};
