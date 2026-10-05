
// --- Entry Point ---
static int kgdb_main(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    if (_isatty(_fileno(stdout)) == 0) {
        DisableColor();
    }

    EnableVTMode();

    WsaContext wsa;
    if (!wsa.IsReady()) {
        std::cerr << RED << "[-] Winsock initialization failed." << RESET << "\n";
        return 1;
    }

    KgdbShell shell;

    bool localFlag = false;
    std::string remoteWinTarget = "";
    std::string dumpFile = "";
    std::string azureTarget = "";
    std::string bsdTarget = "";
    std::string remoteTarget = "";
    std::string hypervPipe = "";
    bool wslFlag = false;
    std::string wslDistro = "";
    std::string execScript = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            KgdbShell::PrintCliHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "kgdb v4.0.0\n";
            return 0;
        } else if (arg == "--no-color") {
            DisableColor();
        } else if (arg == "--local") {
            localFlag = true;
        } else if (arg == "--remote-win" && i + 1 < argc) {
            remoteWinTarget = argv[++i];
        } else if (arg == "--azure" && i + 1 < argc) {
            azureTarget = argv[++i];
        } else if (arg == "--bsd" && i + 1 < argc) {
            bsdTarget = argv[++i];
        } else if (arg == "--wsl") {
            wslFlag = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') wslDistro = argv[++i];
        } else if (arg == "--hyperv" && i + 1 < argc) {
            hypervPipe = argv[++i];
        } else if (arg == "--remote" && i + 1 < argc) {
            remoteTarget = argv[++i];
        } else if ((arg == "-d" || arg == "--dump") && i + 1 < argc) {
            dumpFile = argv[++i];
        } else if ((arg == "-e" || arg == "--exec") && i + 1 < argc) {
            execScript = argv[++i];
        } else if (arg[0] == '-') {
            std::cerr << RED << "[-] Unknown option: " << arg << RESET << "\n";
            KgdbShell::PrintCliHelp();
            return 1;
        } else {
            dumpFile = arg;
        }
    }

    if (localFlag) shell.GetEngine().AttachLocal();
    if (!remoteWinTarget.empty()) shell.GetEngine().AttachRemoteWin(remoteWinTarget);
    if (!azureTarget.empty()) shell.GetEngine().AttachAzure(azureTarget);
    if (!bsdTarget.empty()) shell.GetEngine().AttachBsd(bsdTarget);
    if (wslFlag) shell.GetEngine().AttachWsl(wslDistro);
    if (!hypervPipe.empty()) shell.GetEngine().AttachHyperV(hypervPipe);
    if (!dumpFile.empty()) shell.GetEngine().LoadDumpFile(dumpFile);
    if (!remoteTarget.empty()) {
        size_t colon = remoteTarget.find(':');
        if (colon != std::string::npos) {
            std::string host;
            int port = 0;
            if (!ParseHostPort(remoteTarget, host, port, 0) || !shell.GetEngine().AttachRemoteTcp(host, port)) {
                std::cerr << RED << "[-] Invalid remote endpoint: " << remoteTarget << RESET << "\n";
                return 1;
            }
        }
    }

    if (!execScript.empty()) {
        shell.ExecuteScript(execScript);
        return 0;
    }

    shell.RunInteractive();
    return 0;
}

class KgdbApplication { public: int run(int argc, char* argv[]) const { return kgdb_main(argc, argv); } };
