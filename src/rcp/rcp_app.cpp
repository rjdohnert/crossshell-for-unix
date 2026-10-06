#include "rcp_app.hpp"

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            std::cerr << "\n[SIGNAL] Process interrupted. Cleaning up incomplete temporary files...\n";
            TempFileTracker::Instance().CleanupAll();
            WSACleanup();
            ExitProcess(1);
            return TRUE;
        default:
            return FALSE;
    }
}

int RcpApplication::Run(int argc, wchar_t* argv[]) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(StringUtils::WStringToString(argv[i]));
    }

    if (args.size() < 2) {
        RcpOptions opts;
        opts.PrintHelp(args[0]);
        return 1;
    }

    RcpOptions opt;
    if (!opt.Parse(args)) {
        return 1;
    }

    if (opt.showHelp) {
        opt.PrintHelp(args[0]);
        return 0;
    }

    if (opt.showVersion) {
        opt.PrintVersion();
        return 0;
    }

    WinsockScope winsock;
    if (!winsock.IsInitialized()) {
        std::cerr << "[ERROR] Failed to initialize Winsock 2.2.\n";
        return 1;
    }

    RemotePath src_path = RemotePath::Parse(opt.src);
    RemotePath dst_path = RemotePath::Parse(opt.dst);

    bool success = false;

    if (!src_path.is_remote && dst_path.is_remote) {
        // PUSH MODE
        RcpSocket sock;
        if (sock.Connect(dst_path.host, opt.port)) {
            std::string local_user = StringUtils::GetCurrentLocalUser();
            std::string cmd = "rcp -t ";
            if (opt.recursive) cmd += "-r ";
            if (opt.preserve)  cmd += "-p ";
            cmd += dst_path.path;

            if (sock.SendByte(0) &&
                sock.SendString(local_user) && sock.SendByte(0) &&
                sock.SendString(dst_path.user) && sock.SendByte(0) &&
                sock.SendString(cmd) && sock.SendByte(0) &&
                sock.CheckAck())
            {
                fs::path local_fs_path = StringUtils::Utf8ToPath(src_path.path);
                if (fs::is_directory(local_fs_path)) {
                    if (!opt.recursive) {
                        std::cerr << "[ERROR] Target is a directory! Use -r for recursive copy.\n";
                    } else {
                        success = RcpTransferEngine::SendLocalDirectory(sock, local_fs_path, opt);
                    }
                } else {
                    success = RcpTransferEngine::SendLocalFile(sock, local_fs_path, opt);
                }
            }
        }
    } else if (src_path.is_remote && !dst_path.is_remote) {
        // PULL MODE
        RcpSocket sock;
        if (sock.Connect(src_path.host, opt.port)) {
            std::string local_user = StringUtils::GetCurrentLocalUser();
            std::string cmd = "rcp -f ";
            if (opt.recursive) cmd += "-r ";
            if (opt.preserve)  cmd += "-p ";
            cmd += src_path.path;

            if (sock.SendByte(0) &&
                sock.SendString(local_user) && sock.SendByte(0) &&
                sock.SendString(src_path.user) && sock.SendByte(0) &&
                sock.SendString(cmd) && sock.SendByte(0))
            {
                fs::path local_fs_path = StringUtils::Utf8ToPath(dst_path.path);
                success = RcpTransferEngine::ReceiveStream(sock, local_fs_path, opt);
            }
        }
    } else {
        std::cerr << "[ERROR] Local-to-Local or Remote-to-Remote copy not supported.\n";
    }

    return success ? 0 : 1;
}
