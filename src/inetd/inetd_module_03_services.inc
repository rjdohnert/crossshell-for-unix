// ============================================================================
// 3. BUILT-IN SERVICES & PROCESS LAUNCHER
// ============================================================================

class BuiltinServicesEngine {
public:
    static void HandleTCP(SOCKET client_sock, const std::string& service_name) {
        if (service_name == "echo") {
            char buffer[1024];
            int bytes_read;
            while ((bytes_read = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                send(client_sock, buffer, bytes_read, 0);
            }
        } else if (service_name == "daytime") {
            std::time_t now = std::time(nullptr);
            char time_str[64];
            ctime_s(time_str, sizeof(time_str), &now);
            send(client_sock, time_str, static_cast<int>(strlen(time_str)), 0);
        } else if (service_name == "discard") {
            char buffer[1024];
            while (recv(client_sock, buffer, sizeof(buffer), 0) > 0) {
                // Discard data
            }
        }
        closesocket(client_sock);
    }

    static void HandleUDP(SOCKET sock, const std::string& service_name) {
        char buffer[2048];
        sockaddr_storage client_addr;
        int addr_len = sizeof(client_addr);
        int bytes_read = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (bytes_read <= 0) return;

        if (service_name == "echo") {
            sendto(sock, buffer, bytes_read, 0, reinterpret_cast<sockaddr*>(&client_addr), addr_len);
        } else if (service_name == "daytime") {
            std::time_t now = std::time(nullptr);
            char time_str[64];
            ctime_s(time_str, sizeof(time_str), &now);
            sendto(sock, time_str, static_cast<int>(strlen(time_str)), 0, reinterpret_cast<sockaddr*>(&client_addr), addr_len);
        } else if (service_name == "discard") {
            // Discard datagram
        }
    }
};

class ProcessLauncher {
public:
    static void Launch(SOCKET sock, const ServiceConfig& config) {
        if (!SetHandleInformation(reinterpret_cast<HANDLE>(sock), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to set handle inheritance on socket. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        STARTUPINFOEXA siEx = {};
        PROCESS_INFORMATION pi = {};
        siEx.StartupInfo.cb = sizeof(siEx);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        siEx.StartupInfo.hStdInput  = reinterpret_cast<HANDLE>(sock);
        siEx.StartupInfo.hStdOutput = reinterpret_cast<HANDLE>(sock);
        siEx.StartupInfo.hStdError  = reinterpret_cast<HANDLE>(sock);

        SIZE_T size = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &size);
        std::vector<BYTE> attributeList(size);
        siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeList.data());

        if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size)) {
            InetdLogger::Instance().Log(LOG_ERROR, "InitializeProcThreadAttributeList failed. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        HANDLE handlesToInherit[1] = { reinterpret_cast<HANDLE>(sock) };
        if (!UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       handlesToInherit, sizeof(handlesToInherit), NULL, NULL)) {
            InetdLogger::Instance().Log(LOG_ERROR, "UpdateProcThreadAttribute failed. Win32 Error: " + std::to_string(GetLastError()));
            DeleteProcThreadAttributeList(siEx.lpAttributeList);
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        std::string cmdline;
        if (config.program.find(' ') != std::string::npos && config.program.front() != '"') {
            cmdline = "\"" + config.program + "\"";
        } else {
            cmdline = config.program;
        }
        if (!config.args.empty()) {
            cmdline += " " + config.args;
        }

        std::vector<char> cmd_buffer(cmdline.begin(), cmdline.end());
        cmd_buffer.push_back('\0');

        BOOL success = CreateProcessA(
            NULL,
            cmd_buffer.data(),
            NULL,
            NULL,
            TRUE,
            EXTENDED_STARTUPINFO_PRESENT,
            NULL,
            NULL,
            &siEx.StartupInfo,
            &pi
        );

        DeleteProcThreadAttributeList(siEx.lpAttributeList);

        if (!success) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to launch program '" + config.program + "'. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        InetdLogger::Instance().Log(LOG_DEBUG, "Spawned child process PID " + std::to_string(pi.dwProcessId) + " for service '" + config.name + "'");

        if (config.wait_flag) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }

        if (config.sock_type == "stream") {
            closesocket(sock);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
};

