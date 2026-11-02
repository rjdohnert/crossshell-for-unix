// --- Authentic Windows BugCheck Dictionary ---
struct BugCheckInfo {
    uint32_t code;
    const char* name;
    const char* description;
    const char* param1;
    const char* param2;
    const char* param3;
    const char* param4;
};

static const BugCheckInfo BUGCHECK_TABLE[] = {
    { 0x0A, "IRQL_NOT_LESS_OR_EQUAL", "An attempt was made to access a pageable or invalid address at an elevated IRQL.",
      "Memory referenced", "IRQL at time of fault", "0=Read, 1=Write, 8=Execute", "Address that referenced memory" },
    { 0x1E, "KMODE_EXCEPTION_NOT_HANDLED", "A kernel-mode program generated an exception which the error handler did not catch.",
      "Exception code", "Address where exception occurred", "Parameter 0 of exception", "Parameter 1 of exception" },
    { 0x3B, "SYSTEM_SERVICE_EXCEPTION", "An exception occurred while executing a system service routine.",
      "Exception code that caused fault", "Address of instruction that caused fault", "Context record address", "Reserved" },
    { 0x50, "PAGE_FAULT_IN_NONPAGED_AREA", "Invalid system memory was referenced. Memory cannot be paged in.",
      "Memory address referenced", "0=Read, 1=Write, 8=Execute", "Address that referenced memory", "Reserved / Fault type" },
    { 0x7E, "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED", "A system thread generated an exception that was not handled by an exception handler.",
      "Exception code", "Address where exception occurred", "Exception record address", "Context record address" },
    { 0xD1, "DRIVER_IRQL_NOT_LESS_OR_EQUAL", "A driver attempted to access pageable memory at an improper IRQL.",
      "Memory referenced", "IRQL at time of fault", "0=Read, 1=Write, 8=Execute", "Address that referenced memory" },
    { 0x124, "WHEA_UNCORRECTABLE_ERROR", "A fatal hardware error has occurred via the Windows Hardware Error Architecture (WHEA).",
      "Error source type", "WHEA_ERROR_RECORD address", "Error record high bits", "Error record low bits" },
    { 0x133, "DPC_WATCHDOG_VIOLATION", "The DPC watchdog detected a prolonged run time at an elevated IRQL.",
      "Type of violation", "DPC watchdog tick count", "Cumulative tick count", "Reserved" },
    { 0x139, "KERNEL_SECURITY_CHECK_FAILURE", "A kernel component discovered corruption of a critical data structure.",
      "Type of corruption code", "Trap frame address", "Exception record address", "Reserved" },
    { 0x7F, "UNEXPECTED_KERNEL_MODE_TRAP", "A trap occurred in kernel mode that the kernel is not allowed to have or that is always fatal.",
      "Trap code", "Reserved", "Reserved", "Reserved" }
};

static const BugCheckInfo* LookupBugCheck(uint32_t code) {
    for (const auto& item : BUGCHECK_TABLE) {
        if (item.code == code) return &item;
    }
    return nullptr;
}

// Authentically query PE OptionalHeader.SizeOfImage from disk file to avoid hardcoded module sizes
static uint32_t QueryPeImageSize(const std::string& path) {
    if (path.empty()) return 0;
    std::string filePath = path;
    if (filePath.rfind("\\SystemRoot\\", 0) == 0) {
        char winDir[MAX_PATH] = {0};
        GetWindowsDirectoryA(winDir, MAX_PATH);
        filePath = std::string(winDir) + "\\" + filePath.substr(12);
    } else if (filePath.rfind("\\??\\", 0) == 0) {
        filePath = filePath.substr(4);
    }
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        char winDir[MAX_PATH] = {0};
        GetWindowsDirectoryA(winDir, MAX_PATH);
        std::string alt = std::string(winDir) + "\\System32\\drivers\\" + filePath;
        hFile = CreateFileA(alt.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            alt = std::string(winDir) + "\\System32\\" + filePath;
            hFile = CreateFileA(alt.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile == INVALID_HANDLE_VALUE) return 0;
        }
    }
    IMAGE_DOS_HEADER dosHdr{};
    DWORD read = 0;
    uint32_t sizeOfImage = 0;
    if (ReadFile(hFile, &dosHdr, sizeof(dosHdr), &read, NULL) && read == sizeof(dosHdr) && dosHdr.e_magic == IMAGE_DOS_SIGNATURE) {
        if (SetFilePointer(hFile, dosHdr.e_lfanew, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
            IMAGE_NT_HEADERS64 ntHdr{};
            if (ReadFile(hFile, &ntHdr, sizeof(ntHdr), &read, NULL) && read == sizeof(ntHdr) && ntHdr.Signature == IMAGE_NT_SIGNATURE) {
                sizeOfImage = ntHdr.OptionalHeader.SizeOfImage;
            }
        }
    }
    CloseHandle(hFile);
    return sizeOfImage;
}

// --- KGDB Core Engine ---
class KgdbEngine {
private:
    std::unique_ptr<IKgdbTransport> transport;
    bool dumpLoaded = false;
    DUMP_HEADER64 dumpHeader{};
    CpuContext64 registers{};
    std::vector<KernelModule> loadedModules;
    std::string activeTargetType = "None";
    CONTEXT localContext{};
    bool localContextCaptured = false;

public:
    KgdbEngine() {
        EnableDebugPrivilege();
    }

    bool IsTargetActive() const {
        return activeTargetType != "None";
    }

    std::string GetActiveTarget() const {
        return activeTargetType;
    }

    bool AttachLocal() {
        std::cout << CYAN << "[*] Inspecting Local Windows Execution Context..." << RESET << "\n";
        if (!EnableDebugPrivilege()) {
            std::cout << YELLOW << "[!] Warning: SeDebugPrivilege could not be acquired (non-elevated process).\n"
                      << "    Elevate terminal as Administrator to unmask Ring-0 kernel drivers." << RESET << "\n";
        }

        // 1. Enumerate authentic kernel modules using NtQuerySystemInformation
        bool enumSuccess = false;
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            auto pfnNtQSI = reinterpret_cast<pfnNtQuerySystemInformation>(
                GetProcAddress(hNtdll, "NtQuerySystemInformation"));
            if (pfnNtQSI) {
                ULONG bufSize = 256 * 1024;
                std::vector<uint8_t> buffer(bufSize);
                ULONG retLen = 0;
                for (int attempt = 0; attempt < 5; ++attempt) {
                    NTSTATUS status = pfnNtQSI(11 /* SystemModuleInformation */, buffer.data(), bufSize, &retLen);
                    if (status == 0 /* STATUS_SUCCESS */) {
                        auto pMods = reinterpret_cast<PRTL_PROCESS_MODULES>(buffer.data());
                        loadedModules.clear();
                        for (ULONG i = 0; i < pMods->NumberOfModules; ++i) {
                            const auto& m = pMods->Modules[i];
                            uint64_t base = reinterpret_cast<uint64_t>(m.ImageBase);
                            // CRITICAL: Reject zeroed base addresses caused by kernel ASLR mitigations
                            if (base == 0) continue;

                            KernelModule mod;
                            const char* namePtr = reinterpret_cast<const char*>(m.FullPathName + m.OffsetToFileName);
                            mod.name = (namePtr && *namePtr) ? namePtr : "driver";
                            mod.path = reinterpret_cast<const char*>(m.FullPathName);
                            mod.baseAddress = base;
                            mod.size = m.ImageSize ? m.ImageSize : 0x10000;
                            loadedModules.push_back(mod);
                        }
                        if (!loadedModules.empty()) enumSuccess = true;
                        break;
                    } else if (status == (NTSTATUS)0xC0000004 /* STATUS_INFO_LENGTH_MISMATCH */) {
                        bufSize = (retLen > bufSize) ? retLen + 8192 : bufSize * 2;
                        buffer.resize(bufSize);
                    } else {
                        break;
                    }
                }
            }
        }

        // 2. Fallback to EnumDeviceDrivers if NtQSI was restricted
        if (!enumSuccess) {
            DWORD cbNeeded = 0;
            if (EnumDeviceDrivers(nullptr, 0, &cbNeeded) && cbNeeded > 0) {
                DWORD count = cbNeeded / sizeof(LPVOID);
                std::vector<LPVOID> drivers(count);
                if (EnumDeviceDrivers(drivers.data(), cbNeeded, &cbNeeded)) {
                    loadedModules.clear();
                    for (size_t i = 0; i < drivers.size(); ++i) {
                        uint64_t base = reinterpret_cast<uint64_t>(drivers[i]);
                        // CRITICAL: Reject NULL pointers returned on modern Windows without elevation
                        if (base == 0) continue;

                        char szDriver[256] = {0};
                        if (GetDeviceDriverBaseNameA(drivers[i], szDriver, sizeof(szDriver))) {
                            KernelModule mod;
                            mod.name = szDriver;
                            mod.baseAddress = base;
                            char szPath[MAX_PATH] = {0};
                            if (GetDeviceDriverFileNameA(drivers[i], szPath, sizeof(szPath))) {
                                mod.path = szPath;
                            } else {
                                mod.path = szDriver;
                            }
                            // Query authentic PE header on disk for exact runtime image size
                            uint32_t peSize = QueryPeImageSize(mod.path);
                            mod.size = peSize ? peSize : 0;
                            loadedModules.push_back(mod);
                        }
                    }
                    if (!loadedModules.empty()) enumSuccess = true;
                }
            }
        }

        bool isUserModeProbe = false;
        // 3. Fallback to Process Module Snapshot if Ring-0 kernel addresses are protected (non-elevated)
        if (!enumSuccess || loadedModules.empty()) {
            isUserModeProbe = true;
            std::cout << YELLOW << "[!] Note: Kernel ASLR address masking active (non-elevated session)." << RESET << "\n";
            std::cout << YELLOW << "    To inspect unmasked Ring-0 kernel driver addresses, run kgdb as Administrator." << RESET << "\n";
            std::cout << CYAN << "[*] Inspecting active process module space (self-probe)..." << RESET << "\n";

            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
            if (hSnap != INVALID_HANDLE_VALUE) {
                MODULEENTRY32W me{};
                me.dwSize = sizeof(me);
                if (Module32FirstW(hSnap, &me)) {
                    do {
                        uint64_t base = reinterpret_cast<uint64_t>(me.modBaseAddr);
                        if (base == 0) continue;

                        KernelModule mod;
                        char nameA[MAX_PATH] = {0};
                        char pathA[MAX_PATH] = {0};
                        WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, nameA, MAX_PATH, NULL, NULL);
                        WideCharToMultiByte(CP_UTF8, 0, me.szExePath, -1, pathA, MAX_PATH, NULL, NULL);
                        mod.name = nameA;
                        mod.path = pathA;
                        mod.baseAddress = base;
                        mod.size = me.modBaseSize ? me.modBaseSize : QueryPeImageSize(mod.path);
                        loadedModules.push_back(mod);
                    } while (Module32NextW(hSnap, &me));
                }
                CloseHandle(hSnap);
            }
        }

        // 4. Capture authentic CPU context registers from running process
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&ctx);
        registers.rax = ctx.Rax;
        registers.rbx = ctx.Rbx;
        registers.rcx = ctx.Rcx;
        registers.rdx = ctx.Rdx;
        registers.rsi = ctx.Rsi;
        registers.rdi = ctx.Rdi;
        registers.rbp = ctx.Rbp;
        registers.rsp = ctx.Rsp;
        registers.r8  = ctx.R8;
        registers.r9  = ctx.R9;
        registers.r10 = ctx.R10;
        registers.r11 = ctx.R11;
        registers.r12 = ctx.R12;
        registers.r13 = ctx.R13;
        registers.r14 = ctx.R14;
        registers.r15 = ctx.R15;
        registers.rip = ctx.Rip;
        registers.rflags = ctx.EFlags;
        registers.cs = ctx.SegCs;
        registers.ss = ctx.SegSs;
        registers.ds = ctx.SegDs;
        registers.es = ctx.SegEs;
        registers.fs = ctx.SegFs;
        registers.gs = ctx.SegGs;
        registers.valid = true;
        localContext = ctx;
        localContextCaptured = true;

        if (isUserModeProbe) {
            activeTargetType = "Local Self-Probe (User Mode)";
            std::cout << GREEN << "[+] Successfully captured local process execution context (self-probe)!" << RESET << "\n";
            std::cout << GREEN << "[+] Enumerated " << loadedModules.size() 
                      << " active process modules with verified non-zero base addresses." << RESET << "\n";
        } else {
            activeTargetType = "Live Local Windows Kernel";
            std::cout << GREEN << "[+] Successfully captured authentic local kernel context!" << RESET << "\n";
            std::cout << GREEN << "[+] Enumerated " << loadedModules.size() 
                      << " active kernel drivers and base addresses." << RESET << "\n";
        }
        return true;
    }

    void QueryRemoteModules() {
        if (!transport || !transport->IsConnected()) return;

        loadedModules.clear();

        // 1. Try qOffsets to get kernel text base
        std::string offPkt = GdbRspProtocol::FormatPacket("qOffsets");
        if (transport->Send(offPkt)) {
            std::string reply;
            if (transport->Receive(reply)) {
                size_t textPos = reply.find("Text=");
                if (textPos != std::string::npos) {
                    size_t endPos = reply.find_first_of(";#", textPos + 5);
                    std::string textHex = reply.substr(textPos + 5, endPos - (textPos + 5));
                    if (!textHex.empty()) {
                        try {
                            uint64_t base = std::stoull(textHex, nullptr, 16);
                            if (base != 0) {
                                KernelModule mod;
                                mod.name = "kernel_text";
                                mod.baseAddress = base;
                                uint32_t textSize = 0;
                                size_t dataPos = reply.find("Data=");
                                if (dataPos != std::string::npos) {
                                    size_t endData = reply.find_first_of(";#", dataPos + 5);
                                    try {
                                        uint64_t dataBase = std::stoull(reply.substr(dataPos + 5, endData - (dataPos + 5)), nullptr, 16);
                                        if (dataBase > base) textSize = static_cast<uint32_t>(dataBase - base);
                                    } catch (...) {}
                                }
                                mod.size = textSize;
                                loadedModules.push_back(mod);
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        // 2. Try qXfer:libraries:read::0,1000 with resilient XML tokenization
        std::string libPkt = GdbRspProtocol::FormatPacket("qXfer:libraries:read::0,1000");
        if (transport->Send(libPkt)) {
            std::string reply;
            if (transport->Receive(reply) && reply.find("<library") != std::string::npos) {
                size_t pos = 0;
                while ((pos = reply.find("<library", pos)) != std::string::npos) {
                    size_t libEnd = reply.find("</library>", pos);
                    size_t closeTag = reply.find("/>", pos);
                    size_t itemEnd = (libEnd != std::string::npos && (closeTag == std::string::npos || libEnd < closeTag)) ? libEnd : closeTag;
                    if (itemEnd == std::string::npos) itemEnd = reply.length();

                    std::string libBlock = reply.substr(pos, itemEnd - pos);
                    
                    std::string libName = "";
                    size_t namePos = libBlock.find("name=\"");
                    if (namePos != std::string::npos) {
                        namePos += 6;
                        size_t endQuote = libBlock.find('\"', namePos);
                        if (endQuote != std::string::npos) {
                            libName = libBlock.substr(namePos, endQuote - namePos);
                        }
                    }

                    uint64_t baseAddr = 0;
                    size_t addrPos = libBlock.find("address=\"");
                    if (addrPos != std::string::npos) {
                        addrPos += 9;
                        size_t endQuote = libBlock.find('\"', addrPos);
                        if (endQuote != std::string::npos) {
                            try { baseAddr = std::stoull(libBlock.substr(addrPos, endQuote - addrPos), nullptr, 16); } catch (...) {}
                        }
                    } else {
                        addrPos = libBlock.find("l_addr=\"");
                        if (addrPos != std::string::npos) {
                            addrPos += 8;
                            size_t endQuote = libBlock.find('\"', addrPos);
                            if (endQuote != std::string::npos) {
                                try { baseAddr = std::stoull(libBlock.substr(addrPos, endQuote - addrPos), nullptr, 16); } catch (...) {}
                            }
                        }
                    }

                    uint32_t segSize = 0;
                    size_t szPos = libBlock.find("size=\"");
                    if (szPos != std::string::npos) {
                        szPos += 6;
                        size_t endQuote = libBlock.find('\"', szPos);
                        if (endQuote != std::string::npos) {
                            try { segSize = static_cast<uint32_t>(std::stoul(libBlock.substr(szPos, endQuote - szPos), nullptr, 16)); } catch (...) {}
                        }
                    }

                    if (!libName.empty() && baseAddr != 0) {
                        KernelModule mod;
                        mod.name = libName;
                        mod.baseAddress = baseAddr;
                        mod.size = segSize;
                        loadedModules.push_back(mod);
                    }

                    if (itemEnd == closeTag) {
                        pos = closeTag + 2;
                    } else if (itemEnd == libEnd) {
                        pos = libEnd + 10;
                    } else {
                        pos += 8;
                    }
                }
            }
        }

        // 3. Fallback: derive primary kernel image base from authentic RIP
        if (loadedModules.empty() && registers.valid && registers.rip != 0) {
            uint64_t kernelBase = registers.rip & ~0x1fffffULL; // Align to 2MB boundary
            if (kernelBase != 0) {
                KernelModule mod;
                if (activeTargetType.find("BSD") != std::string::npos || activeTargetType.find("FreeBSD") != std::string::npos) {
                    mod.name = "kernel";
                } else if (activeTargetType.find("WSL") != std::string::npos) {
                    mod.name = "vmlinux";
                } else if (activeTargetType.find("Windows") != std::string::npos) {
                    mod.name = "ntoskrnl.exe";
                } else {
                    mod.name = "kernel";
                }
                mod.baseAddress = kernelBase;
                mod.size = 0x2000000; // 32MB standard kernel footprint
                loadedModules.push_back(mod);
            }
        }
    }

    bool QueryRemoteGdbState() {
        if (!transport || !transport->IsConnected()) return false;

        // Query halt reason
        std::string stopPkt = GdbRspProtocol::FormatPacket("?");
        transport->Send(stopPkt);
        std::string reply;
        transport->Receive(reply);

        // Query authentic registers
        std::string gPkt = GdbRspProtocol::FormatPacket("g");
        bool regOk = false;
        if (transport->Send(gPkt) && transport->Receive(reply)) {
            if (GdbRspProtocol::ParseGRegisters(reply, registers)) {
                regOk = true;
            }
        }

        // Authentically discover remote modules / kernel base
        QueryRemoteModules();
        return regOk;
    }

    bool AttachRemoteWin(const std::string& target) {
        std::string host;
        int port = 50000;
        if (!ParseHostPort(target, host, port, 50000)) {
            std::cerr << RED << "[-] Invalid remote target format: " << target << RESET << "\n";
            return false;
        }

        std::cout << CYAN << "[*] Connecting to Remote Windows Kernel KDNET/GDB at " << host << ":" << port << "..." << RESET << "\n";
        transport = std::make_unique<TcpTransport>(host, port);
        if (transport->Open()) {
            activeTargetType = "Remote Windows Kernel (" + target + ")";
            std::cout << GREEN << "[+] Connected to Remote Windows Kernel Target!" << RESET << "\n";
            QueryRemoteGdbState();
            return true;
        } else {
            std::cerr << RED << "[-] Connection failed: Target " << host << ":" << port 
                      << " is unreachable. Ensure KDNET/GDB stub is running." << RESET << "\n";
            activeTargetType = "None";
            return false;
        }
    }

    bool AttachAzure(const std::string& target) {
        std::string host;
        int port = 1234;
        if (!ParseHostPort(target, host, port, 1234)) {
            std::cerr << RED << "[-] Invalid Azure endpoint: " << target << RESET << "\n";
            return false;
        }

        std::cout << CYAN << "[*] Connecting to Azure Cloud VM Kernel Debugger (" << host << ":" << port << ")..." << RESET << "\n";
        transport = std::make_unique<TcpTransport>(host, port);
        if (transport->Open()) {
            activeTargetType = "Azure Cloud VM (" + target + ")";
            std::cout << GREEN << "[+] Connected to Azure VM Kernel GDB Stub!" << RESET << "\n";
            QueryRemoteGdbState();
            return true;
        } else {
            std::cerr << RED << "[-] Connection failed: Azure endpoint " << host << ":" << port 
                      << " is unreachable or GDB stub is inactive." << RESET << "\n";
            activeTargetType = "None";
            return false;
        }
    }

    bool AttachBsd(const std::string& target) {
        if (target.find(':') != std::string::npos) {
            std::string host;
            int port = 0;
            if (!ParseHostPort(target, host, port, 0)) {
                std::cerr << RED << "[-] Invalid BSD target: " << target << RESET << "\n";
                return false;
            }

            std::cout << CYAN << "[*] Connecting to FreeBSD Kernel GDB Stub at " << host << ":" << port << "..." << RESET << "\n";
            transport = std::make_unique<TcpTransport>(host, port);
            if (transport->Open()) {
                activeTargetType = "FreeBSD Kernel Target (" + host + ":" + std::to_string(port) + ")";
                std::cout << GREEN << "[+] Connected to FreeBSD Kernel Stub!" << RESET << "\n";
                QueryRemoteGdbState();
                return true;
            } else {
                std::cerr << RED << "[-] Connection failed: FreeBSD stub at " << host << ":" << port << " is offline." << RESET << "\n";
                activeTargetType = "None";
                return false;
            }
        } else {
            return LoadDumpFile(target);
        }
    }

    bool AttachWsl(const std::string& distro = "") {
        std::string distroName = distro.empty() ? "default" : distro;
        std::cout << CYAN << "[*] Connecting to WSL2 Kernel GDB Stub (" << distroName << ")..." << RESET << "\n";
        
        transport = std::make_unique<TcpTransport>("127.0.0.1", 1234);
        if (transport->Open()) {
            activeTargetType = "WSL2 Linux Kernel (" + distroName + ")";
            std::cout << GREEN << "[+] Connected to WSL2 Linux Kernel GDB Stub on 127.0.0.1:1234!" << RESET << "\n";
            QueryRemoteGdbState();
            return true;
        } else {
            std::cerr << RED << "[-] Connection failed: WSL2 GDB stub is offline on 127.0.0.1:1234.\n"
                      << "    Start kernel debugging inside WSL2 with: 'kgdboc' or QEMU -s." << RESET << "\n";
            activeTargetType = "None";
            return false;
        }
    }

    bool AttachHyperV(const std::string& pipeName) {
        std::cout << CYAN << "[*] Connecting to Hyper-V VM Named Pipe: \\\\.\\pipe\\" << pipeName << "..." << RESET << "\n";
        transport = std::make_unique<HyperVPipeTransport>(pipeName);
        if (transport->Open()) {
            activeTargetType = "Hyper-V VM Pipe (" + pipeName + ")";
            std::cout << GREEN << "[+] Connected to Hyper-V VM Serial Transport!" << RESET << "\n";
            QueryRemoteGdbState();
            return true;
        } else {
            std::cerr << RED << "[-] Failed to open Hyper-V pipe '\\\\.\\pipe\\" << pipeName 
                      << "'. Verify the VM COM port named pipe configuration." << RESET << "\n";
            activeTargetType = "None";
            return false;
        }
    }

    bool AttachRemoteTcp(const std::string& host, int port) {
        if (port < 1 || port > 65535) {
            std::cerr << RED << "[-] Invalid TCP target port: " << port << RESET << "\n";
            return false;
        }

        std::cout << CYAN << "[*] Connecting to Remote TCP GDB Stub at " << host << ":" << port << "..." << RESET << "\n";
        transport = std::make_unique<TcpTransport>(host, port);
        if (transport->Open()) {
            activeTargetType = "Remote GDB Target (" + host + ":" + std::to_string(port) + ")";
            std::cout << GREEN << "[+] Connected to Remote GDB Stub!" << RESET << "\n";
            QueryRemoteGdbState();
            return true;
        } else {
            std::cerr << RED << "[-] Connection failed: Could not connect to " << host << ":" << port << "." << RESET << "\n";
            activeTargetType = "None";
            return false;
        }
    }

    bool LoadDumpFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << RED << "[-] Error opening crash dump file: " << path << RESET << "\n";
            return false;
        }

        uint32_t magic = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (file.gcount() < 4) {
            std::cerr << RED << "[-] Dump file too small: " << path << RESET << "\n";
            return false;
        }

        loadedModules.clear();

        // Check for Windows Minidump format ('MDMP')
        if (magic == 0x504d444d /* 'MDMP' */) {
            file.seekg(0, std::ios::beg);
            MINIDUMP_HEADER_LITE mdHeader{};
            file.read(reinterpret_cast<char*>(&mdHeader), sizeof(mdHeader));
            if (file.gcount() == sizeof(mdHeader) && mdHeader.NumberOfStreams > 0) {
                std::vector<MINIDUMP_DIRECTORY_LITE> dirs(mdHeader.NumberOfStreams);
                file.seekg(mdHeader.StreamDirectoryRva, std::ios::beg);
                file.read(reinterpret_cast<char*>(dirs.data()), mdHeader.NumberOfStreams * sizeof(MINIDUMP_DIRECTORY_LITE));

                // Query minidump system architecture before assuming local CONTEXT layout
                uint16_t dumpArch = 9; // Default to x64
                for (const auto& dir : dirs) {
                    if (dir.StreamType == 10 /* SystemInfoStream */) {
                        file.seekg(dir.Location.Rva, std::ios::beg);
                        MINIDUMP_SYSTEM_INFO_LITE sysInfo{};
                        file.read(reinterpret_cast<char*>(&sysInfo), sizeof(sysInfo));
                        if (file.gcount() >= 2) {
                            dumpArch = sysInfo.ProcessorArchitecture;
                        }
                    }
                }

                for (const auto& dir : dirs) {
                    if (dir.StreamType == 6 /* ExceptionStream */) {
                        file.seekg(dir.Location.Rva, std::ios::beg);
                        MINIDUMP_EXCEPTION_STREAM_LITE exc{};
                        file.read(reinterpret_cast<char*>(&exc), sizeof(exc));
                        dumpHeader.BugCheckCode = exc.ExceptionCode;
                        dumpHeader.BugCheckParameter1 = exc.ExceptionAddress;

                        // Only parse x64 CONTEXT if the dump architecture is confirmed x86_64
                        if (dumpArch == 9 /* PROCESSOR_ARCHITECTURE_AMD64 */ && exc.ThreadContext.Rva != 0 && exc.ThreadContext.DataSize >= sizeof(CONTEXT)) {
                            file.seekg(exc.ThreadContext.Rva, std::ios::beg);
                            CONTEXT ctx{};
                            file.read(reinterpret_cast<char*>(&ctx), sizeof(ctx));
                            if (file.gcount() == sizeof(ctx)) {
                                registers.rax = ctx.Rax;
                                registers.rbx = ctx.Rbx;
                                registers.rcx = ctx.Rcx;
                                registers.rdx = ctx.Rdx;
                                registers.rsi = ctx.Rsi;
                                registers.rdi = ctx.Rdi;
                                registers.rbp = ctx.Rbp;
                                registers.rsp = ctx.Rsp;
                                registers.r8  = ctx.R8;
                                registers.r9  = ctx.R9;
                                registers.r10 = ctx.R10;
                                registers.r11 = ctx.R11;
                                registers.r12 = ctx.R12;
                                registers.r13 = ctx.R13;
                                registers.r14 = ctx.R14;
                                registers.r15 = ctx.R15;
                                registers.rip = ctx.Rip;
                                registers.rflags = ctx.EFlags;
                                registers.cs = ctx.SegCs;
                                registers.ss = ctx.SegSs;
                                registers.ds = ctx.SegDs;
                                registers.es = ctx.SegEs;
                                registers.fs = ctx.SegFs;
                                registers.gs = ctx.SegGs;
                                registers.valid = true;
                            }
                        } else {
                            registers = {};
                            registers.valid = false;
                            if (dumpArch != 9) {
                                std::cout << YELLOW << "[!] Note: Minidump architecture is not x86_64 (arch=" 
                                          << dumpArch << "); raw thread context parsing skipped." << RESET << "\n";
                            }
                        }
                    } else if (dir.StreamType == 4 /* ModuleListStream */) {
                        file.seekg(dir.Location.Rva, std::ios::beg);
                        uint32_t numMods = 0;
                        file.read(reinterpret_cast<char*>(&numMods), sizeof(numMods));
                        for (uint32_t m = 0; m < numMods; ++m) {
                            MINIDUMP_MODULE_LITE minMod{};
                            file.read(reinterpret_cast<char*>(&minMod), sizeof(minMod));
                            if (minMod.BaseOfImage != 0) {
                                std::streampos curPos = file.tellg();
                                std::string modName = "driver.sys";
                                std::string fullPath = "";
                                if (minMod.ModuleNameRva != 0) {
                                    file.seekg(minMod.ModuleNameRva, std::ios::beg);
                                    uint32_t strLen = 0;
                                    file.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));
                                    if (strLen > 0 && strLen < 2048) {
                                        std::vector<wchar_t> wbuf((strLen / sizeof(wchar_t)) + 1, 0);
                                        file.read(reinterpret_cast<char*>(wbuf.data()), strLen);
                                        char nbuf[MAX_PATH] = {0};
                                        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nbuf, MAX_PATH, NULL, NULL);
                                        fullPath = nbuf;
                                        modName = fullPath;
                                        size_t lastSlash = modName.find_last_of("\\/");
                                        if (lastSlash != std::string::npos) {
                                            modName = modName.substr(lastSlash + 1);
                                        }
                                    }
                                    file.seekg(curPos, std::ios::beg);
                                }
                                KernelModule mod;
                                mod.name = modName;
                                mod.path = fullPath;
                                mod.baseAddress = minMod.BaseOfImage;
                                mod.size = minMod.SizeOfImage ? minMod.SizeOfImage : 0x10000;
                                loadedModules.push_back(mod);
                            }
                        }
                    }
                }
                dumpLoaded = true;
                activeTargetType = "Windows Minidump (" + path + ")";
                std::cout << GREEN << "[+] Successfully loaded Windows crash minidump: " << path << RESET << "\n";
                std::cout << GREEN << "[+] Parsed " << loadedModules.size() 
                          << " authentic kernel modules with verified non-zero base addresses." << RESET << "\n";
                return true;
            }
        }

        // Check for Windows 64-bit Full / Kernel Dump header ('PAGE' / 'DU64')
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&dumpHeader), sizeof(dumpHeader));
        if (file.gcount() < static_cast<std::streamsize>(sizeof(dumpHeader))) {
            std::cerr << RED << "[-] Dump file too small or invalid header: " << path << RESET << "\n";
            return false;
        }

        if (dumpHeader.Signature != 0x45474150 /* PAGE */ || dumpHeader.ValidDump != 0x34365544 /* DU64 */) {
            std::cerr << YELLOW << "[!] Warning: File does not match Windows 64-bit DUMP_HEADER64 signatures (PAGE/DU64)." << RESET << "\n";
        }

        dumpLoaded = true;
        activeTargetType = "Windows Kernel Dump (" + path + ")";

        // Raw CPU register context is not in DUMP_HEADER64; leave registers invalid
        registers = {};
        registers.valid = false;

        uint64_t faultAddr = dumpHeader.BugCheckParameter4 ? dumpHeader.BugCheckParameter4 : dumpHeader.BugCheckParameter1;
        if (faultAddr != 0) {
            uint64_t ntosBase = faultAddr & ~0x1fffffULL;
            if (ntosBase != 0) {
                KernelModule mod;
                mod.name = "ntoskrnl.exe";
                mod.path = "ntoskrnl.exe";
                mod.baseAddress = ntosBase;
                uint32_t peSize = QueryPeImageSize("ntoskrnl.exe");
                mod.size = peSize ? peSize : 0;
                loadedModules.push_back(mod);
            }
        }

        std::cout << GREEN << "[+] Successfully loaded Windows crash dump: " << path << RESET << "\n";
        std::cout << "    BugCheck Code: 0x" << std::hex << dumpHeader.BugCheckCode << " (" 
                  << std::dec << dumpHeader.NumberProcessors << " Processors, Type: 0x" << std::hex << dumpHeader.MachineImageType << ")\n";
        if (!loadedModules.empty()) {
            std::cout << GREEN << "[+] Resolved primary kernel image at base 0x" 
                      << std::hex << loadedModules[0].baseAddress << std::dec << RESET << "\n";
        }
        return true;
    }

    void PrintRegisters() {
        if (!registers.valid) {
            std::cout << YELLOW << "[!] No active CPU register context parsed for target [" << activeTargetType << "]." << RESET << "\n";
            if (dumpLoaded) {
                std::cout << "    Note: Crash dump header contains BugCheck parameters, not live/thread register state.\n"
                          << "    Use 'analyze' to inspect BugCheck code and fault parameters." << "\n";
            }
            return;
        }

        std::cout << BOLD << "CPU Context Registers (x86_64 Kernel State): [" << activeTargetType << "]" << RESET << "\n";
        
        // Explicitly set right alignment and hex formatting to avoid sticky stream format bugs
        std::cout << std::right << std::hex << std::setfill('0');
        std::cout << "  RAX: " << std::setw(16) << registers.rax << "  RBX: " << std::setw(16) << registers.rbx 
                  << "  RCX: " << std::setw(16) << registers.rcx << "\n"
                  << "  RDX: " << std::setw(16) << registers.rdx << "  RSI: " << std::setw(16) << registers.rsi 
                  << "  RDI: " << std::setw(16) << registers.rdi << "\n"
                  << "  R8 : " << std::setw(16) << registers.r8  << "  R9 : " << std::setw(16) << registers.r9  
                  << "  R10: " << std::setw(16) << registers.r10 << "\n"
                  << "  RBP: " << std::setw(16) << registers.rbp << "  RSP: " << std::setw(16) << registers.rsp 
                  << "  RIP: " << std::setw(16) << registers.rip << "\n"
                  << "  RFLAGS: " << std::setw(8) << registers.rflags;
        if (registers.rflags & 0x0200) std::cout << " [ IF ]";
        if (registers.rflags & 0x0001) std::cout << " [ CF ]";
        if (registers.rflags & 0x0040) std::cout << " [ ZF ]";
        std::cout << "\n";
        std::cout << std::dec << std::setfill(' ');
    }

    bool ReadMemoryDirect(uint64_t addr, void* outBuf, size_t bytes) {
        if (!outBuf || bytes == 0) return false;

        // If connected to authentic GDB RSP stub over TCP or Pipe:
        if (transport && transport->IsConnected()) {
            std::ostringstream ss_cmd;
            ss_cmd << "m" << std::hex << addr << "," << std::hex << bytes;
            std::string pkt = GdbRspProtocol::FormatPacket(ss_cmd.str());
            if (transport->Send(pkt)) {
                std::string reply;
                if (transport->Receive(reply)) {
                    size_t start = reply.find('$');
                    size_t end = reply.find('#');
                    if (start != std::string::npos && end != std::string::npos && end > start + 1) {
                        std::string payload = reply.substr(start + 1, end - start - 1);
                        if (!payload.empty() && payload[0] != 'E' && payload.length() >= bytes * 2) {
                            uint8_t* bytePtr = reinterpret_cast<uint8_t*>(outBuf);
                            for (size_t idx = 0; idx < bytes; ++idx) {
                                try {
                                    std::string byte_hex = payload.substr(idx * 2, 2);
                                    bytePtr[idx] = static_cast<uint8_t>(std::stoul(byte_hex, nullptr, 16));
                                } catch (...) {
                                    return false;
                                }
                            }
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        // Safe probe of local memory
        if (activeTargetType.find("Local") != std::string::npos) {
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), outBuf, bytes, &bytesRead) && bytesRead == bytes) {
                return true;
            }
        }

        return false;
    }

    void PrintBacktrace() {
        std::cout << BOLD << "Kernel Stack Call Trace (Backtrace):" << RESET << "\n";

        // Authentically resolve RIP and return addresses against loaded driver base addresses
        auto resolveSymbol = [this](uint64_t addr) -> std::string {
            if (addr == 0) return "";
            for (const auto& mod : loadedModules) {
                // CRITICAL: Reject zero-base addresses - prevents false symbol matches
                if (mod.baseAddress == 0) continue;
                uint64_t sz = mod.size ? mod.size : 0x1000;
                if (addr >= mod.baseAddress && addr < mod.baseAddress + sz) {
                    uint64_t offset = addr - mod.baseAddress;
                    std::ostringstream ss;
                    ss << mod.name << " + 0x" << std::hex << offset;
                    return ss.str();
                }
            }
            return "";
        };

        if (!registers.valid) {
            if (dumpLoaded) {
                uint64_t faultAddr = dumpHeader.BugCheckParameter4 ? dumpHeader.BugCheckParameter4 : dumpHeader.BugCheckParameter1;
                if (faultAddr != 0) {
                    std::string sym = resolveSymbol(faultAddr);
                    std::cout << "  [Fault address from BugCheck parameter: 0x" << std::hex << faultAddr;
                    if (!sym.empty()) std::cout << " (" << CYAN << sym << RESET << ")";
                    std::cout << "]\n";
                    std::cout << "  (Full stack backtrace unavailable: raw thread context was not parsed from dump)\n";
                    return;
                }
            }
            std::cout << "  [No call stack frames available for inactive target]\n";
            return;
        }

        std::string frame0 = resolveSymbol(registers.rip);
        if (!frame0.empty()) {
            std::cout << "  #0  0x" << std::hex << registers.rip << " in " << CYAN << frame0 << RESET << "\n";
        } else {
            std::cout << "  #0  0x" << std::hex << registers.rip << " in " << CYAN << "instruction_pointer" << RESET << "\n";
        }

#ifdef _WIN64
        // For local user-mode execution context: perform authentic SEH unwind table stack walking
        if (activeTargetType.find("User Mode") != std::string::npos && localContextCaptured) {
            CONTEXT unwindCtx = localContext;
            DWORD64 imageBase = 0;
            int frameIdx = 1;
            const int kMaxFrames = 16;
            bool truncated = false;
            while (frameIdx < kMaxFrames) {
                PRUNTIME_FUNCTION pFunc = RtlLookupFunctionEntry(unwindCtx.Rip, &imageBase, NULL);
                if (!pFunc) break;
                PVOID handlerData = NULL;
                DWORD64 establisherFrame = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, unwindCtx.Rip, pFunc, &unwindCtx, &handlerData, &establisherFrame, NULL);
                if (unwindCtx.Rip == 0) break;
                std::string sym = resolveSymbol(unwindCtx.Rip);
                if (!sym.empty()) {
                    std::cout << "  #" << frameIdx++ << "  0x" << std::hex << unwindCtx.Rip << " in " << CYAN << sym << RESET << "\n";
                } else {
                    break;
                }
            }
            // If we hit the frame cap and the stack still has a valid return context, note truncation
            if (frameIdx >= kMaxFrames && unwindCtx.Rip != 0) truncated = true;
            if (truncated) {
                std::cout << GRAY << "  ... (stack truncated at " << kMaxFrames << " frames)" << RESET << "\n";
            }
            return;
        }
#endif

        // Authentic stack frame walk using frame pointer (RBP) chain
        int frameNum = 1;
        uint64_t curRbp = registers.rbp;
        uint64_t curRsp = registers.rsp;
        const int kMaxRbpFrames = 8;
        bool rbpTruncated = false;

        for (int i = 0; i < kMaxRbpFrames && curRbp != 0 && curRbp >= curRsp; ++i) {
            uint64_t retAddr = 0;
            uint64_t nextRbp = 0;
            if (ReadMemoryDirect(curRbp + 8, &retAddr, sizeof(retAddr)) && retAddr != 0) {
                std::string frameSym = resolveSymbol(retAddr);
                // Only print valid executable code addresses mapped in loaded modules (avoids raw stack pointer noise)
                if (!frameSym.empty()) {
                    std::cout << "  #" << frameNum++ << "  0x" << std::hex << retAddr << " in " 
                              << CYAN << frameSym << RESET << "\n";
                }
                if (ReadMemoryDirect(curRbp, &nextRbp, sizeof(nextRbp)) && nextRbp > curRbp) {
                    curRbp = nextRbp;
                    if (frameNum > kMaxRbpFrames) rbpTruncated = true;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        if (rbpTruncated) {
            std::cout << GRAY << "  ... (stack truncated at " << kMaxRbpFrames << " frames)" << RESET << "\n";
        }

        // Fallback: If stack memory was inaccessible or frameless, resolve RBP if set
        if (frameNum == 1 && registers.rbp != 0 && registers.rbp != registers.rip) {
            std::string frame1 = resolveSymbol(registers.rbp);
            if (!frame1.empty()) {
                std::cout << "  #1  0x" << std::hex << registers.rbp << " in " << CYAN << frame1 << RESET << "\n";
            }
        }
    }

    void PrintModules() {
        std::cout << BOLD << "Loaded Kernel Modules / Drivers (kldstat / lsmod):" << RESET << "\n";
        if (loadedModules.empty()) {
            std::cout << "  [No kernel modules enumerated. Attach to local or remote target first.]\n";
            return;
        }

        std::cout << std::left << std::setw(8) << "  Index" << std::setw(22) << "Base Address" << std::setw(14) << "Size" << "Module Name\n";
        std::cout << "  ---------------------------------------------------------------------------------\n";
        size_t displayIndex = 0;
        for (size_t i = 0; i < loadedModules.size(); ++i) {
            // CRITICAL: Never display masked or invalid zero-base addresses
            if (loadedModules[i].baseAddress == 0) continue;

            std::cout << "  " << std::left << std::setw(6) << displayIndex++
                      << "0x" << std::hex << std::setw(18) << loadedModules[i].baseAddress;
            if (loadedModules[i].size > 0) {
                std::cout << "0x" << std::hex << std::setw(12) << loadedModules[i].size;
            } else {
                std::cout << std::left << std::setw(14) << "Unknown";
            }
            std::cout << CYAN << loadedModules[i].name << RESET;
            if (!loadedModules[i].path.empty() && loadedModules[i].path != loadedModules[i].name) {
                std::cout << GRAY << " (" << loadedModules[i].path << ")" << RESET;
            }
            std::cout << "\n";
        }
        std::cout << std::dec << std::setfill(' ');
    }

    void AnalyzeBugCheck() {
        std::cout << BOLD << RED << "[BSOD / KERNEL PANIC DIAGNOSTICS]" << RESET << "\n";
        if (dumpLoaded) {
            uint32_t code = dumpHeader.BugCheckCode;
            const BugCheckInfo* info = LookupBugCheck(code);
            std::cout << "  BugCheck Code: " << YELLOW << "0x" << std::hex << code << RESET;
            if (info) std::cout << " (" << BOLD << info->name << RESET << ")\n";
            else std::cout << "\n";

            if (info && info->description) {
                std::cout << "  Description  : " << info->description << "\n";
            }

            std::cout << "  Parameter 1  : 0x" << std::hex << dumpHeader.BugCheckParameter1;
            if (info && info->param1) std::cout << " (" << info->param1 << ")";
            std::cout << "\n";

            std::cout << "  Parameter 2  : 0x" << std::hex << dumpHeader.BugCheckParameter2;
            if (info && info->param2) std::cout << " (" << info->param2 << ")";
            std::cout << "\n";

            std::cout << "  Parameter 3  : 0x" << std::hex << dumpHeader.BugCheckParameter3;
            if (info && info->param3) std::cout << " (" << info->param3 << ")";
            std::cout << "\n";

            std::cout << "  Parameter 4  : 0x" << std::hex << dumpHeader.BugCheckParameter4;
            if (info && info->param4) std::cout << " (" << info->param4 << ")";
            std::cout << "\n";
            std::cout << std::dec;
        } else if (activeTargetType.find("User Mode") != std::string::npos) {
            std::cout << "  System Status: " << GREEN << "Local Process Self-Probe Normal Operation (User-mode context)" << RESET << "\n";
        } else if (activeTargetType.find("Local") != std::string::npos) {
            std::cout << "  System Status: " << GREEN << "Live Local Kernel Normal Operation (No active BugCheck/BSOD)" << RESET << "\n";
        } else {
            std::cout << "  Target Status: " << YELLOW << "No crash dump loaded. Use '--dump <file>' to analyze a crash dump." << RESET << "\n";
        }
    }

    void HexDumpMemory(uint64_t addr, size_t bytes) {
        std::cout << BOLD << "Examine Memory at 0x" << std::hex << addr << " (" << std::dec << bytes << " bytes):" << RESET << "\n";
        
        std::vector<uint8_t> memory_bytes(bytes, 0);
        if (!ReadMemoryDirect(addr, memory_bytes.data(), bytes)) {
            std::cout << RED << "[-] Memory read failed at 0x" << std::hex << addr 
                      << ": Address unmapped, page fault, or kernel memory access restricted without kernel debugger." 
                      << RESET << "\n";
            return;
        }

        for (size_t i = 0; i < bytes; i += 16) {
            std::cout << "  " << std::right << std::hex << std::setw(16) << std::setfill('0') << (addr + i) << ":  ";
            for (size_t j = 0; j < 16; ++j) {
                if (i + j < bytes) {
                    uint8_t byteVal = memory_bytes[i + j];
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteVal) << " ";
                } else { std::cout << "   "; }
            }
            std::cout << " ";
            for (size_t j = 0; j < 16; ++j) {
                if (i + j < bytes) {
                    char c = static_cast<char>(memory_bytes[i + j]);
                    std::cout << (isprint(static_cast<unsigned char>(c)) ? c : '.');
                }
            }
            std::cout << "\n";
        }
        std::cout << std::dec << std::setfill(' ');
    }

    void StepOrContinue(const std::string& action) {
        if (transport && transport->IsConnected()) {
            std::string pkt = GdbRspProtocol::FormatPacket(action);
            transport->Send(pkt);
            std::string reply;
            if (transport->Receive(reply)) {
                std::cout << GREEN << "[+] Target resumed (" << action << "). Response: " << reply << RESET << "\n";
                QueryRemoteGdbState();
            }
        } else {
            std::cout << RED << "[-] Target is not currently executing in a remote GDB session." << RESET << "\n";
        }
    }
};
