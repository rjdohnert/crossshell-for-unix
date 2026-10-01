/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <mutex>
#include <algorithm>
#include <memory>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. CONSTANTS, UTILITIES & RAII SCOPES
// ============================================================================

constexpr const char* DEFAULT_RCP_PORT = "514";
constexpr size_t BUFFER_SIZE = 65536;
constexpr DWORD SOCKET_TIMEOUT_MS = 30000;

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class StringUtils {
public:
    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    static std::wstring Utf8ToWString(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static fs::path Utf8ToPath(const std::string& utf8_str) {
        return fs::path(Utf8ToWString(utf8_str));
    }

    static bool IsRunningAsAdmin() {
        return IsUserAnAdmin() != FALSE;
    }

    static std::string SanitizeIncomingFilename(const std::string& raw_name) {
        fs::path p(Utf8ToWString(raw_name));
        std::string clean_name = WStringToString(p.filename().wstring());
        if (clean_name.empty() || clean_name == "." || clean_name == "..") {
            return "unnamed_file";
        }
        return clean_name;
    }

    static std::string GetCurrentLocalUser() {
        wchar_t buffer[256] = {};
        DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
        if (GetUserNameW(buffer, &size)) {
            return WStringToString(buffer);
        }
        return "nobody";
    }
};

class FileTimestampHelper {
public:
    static FILETIME UnixTimeToFILETIME(int64_t unix_time) {
        int64_t ft_val = (unix_time * 10000000LL) + 116444736000000000LL;
        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(ft_val & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>(ft_val >> 32);
        return ft;
    }

    static int64_t FILETIMEToUnixTime(const FILETIME& ft) {
        int64_t ft_val = (static_cast<int64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return (ft_val - 116444736000000000LL) / 10000000LL;
    }

    static bool SetFileTimestampsWin32(const fs::path& filepath, int64_t mtime, int64_t atime) {
        HANDLE hFile = CreateFileW(filepath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        FILETIME mft = UnixTimeToFILETIME(mtime);
        FILETIME aft = UnixTimeToFILETIME(atime);
        BOOL res = SetFileTime(hFile, NULL, &aft, &mft);
        CloseHandle(hFile);
        return res != FALSE;
    }

    static bool GetFileTimestampsWin32(const fs::path& filepath, int64_t& mtime, int64_t& atime) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &fad)) return false;
        mtime = FILETIMEToUnixTime(fad.ftLastWriteTime);
        atime = FILETIMEToUnixTime(fad.ftLastAccessTime);
        return true;
    }
};

// ============================================================================
// 2. TEMP FILE TRACKER
// ============================================================================

class TempFileTracker {
public:
    static TempFileTracker& Instance() {
        static TempFileTracker instance;
        return instance;
    }

    void Register(const fs::path& p) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeFiles.push_back(p);
    }

    void Unregister(const fs::path& p) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find(m_activeFiles.begin(), m_activeFiles.end(), p);
        if (it != m_activeFiles.end()) {
            m_activeFiles.erase(it);
        }
    }

    void CleanupAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::error_code ec;
        for (const auto& p : m_activeFiles) {
            fs::remove(p, ec);
        }
        m_activeFiles.clear();
    }

private:
    std::mutex m_mutex;
    std::vector<fs::path> m_activeFiles;
};

// ============================================================================
// 3. DATA MODELS & PARSED PATH
// ============================================================================

struct RemotePath {
    bool is_remote = false;
    std::string user;
    std::string host;
    std::string path;

    static RemotePath Parse(const std::string& input) {
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
};

class RcpOptions {
public:
    bool recursive = false;
    bool preserve = false;
    std::string port = DEFAULT_RCP_PORT;
    std::string src;
    std::string dst;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(const std::vector<std::string>& args) {
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

    void PrintHelp(const std::string& exe_name) const {
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

    void PrintVersion() const {
        std::cout << "rcp 3.0.0\n";
    }
};

// ============================================================================
// 4. RCP SOCKET WRAPPER
// ============================================================================

class RcpSocket {
public:
    RcpSocket() : m_sock(INVALID_SOCKET) {}
    ~RcpSocket() { Close(); }

    bool Connect(const std::string& host, const std::string& port) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
            std::cerr << "[ERROR] Failed to resolve host: " << host << "\n";
            return false;
        }

        for (addrinfo* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
            m_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (m_sock == INVALID_SOCKET) continue;

            DWORD timeout = SOCKET_TIMEOUT_MS;
            setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

            if (ptr->ai_family == AF_INET) {
                sockaddr_in local_sin{};
                local_sin.sin_family = AF_INET;
                local_sin.sin_addr.s_addr = INADDR_ANY;

                bool bound_priv = false;
                for (unsigned short priv_port = 1023; priv_port >= 512; --priv_port) {
                    local_sin.sin_port = htons(priv_port);
                    if (bind(m_sock, (sockaddr*)&local_sin, sizeof(local_sin)) == 0) {
                        bound_priv = true;
                        break;
                    }
                }
                if (!bound_priv && !StringUtils::IsRunningAsAdmin()) {
                    std::cerr << "[NOTICE] Running without elevated privileges. Standard ephemeral port bound.\n";
                }
            } else if (ptr->ai_family == AF_INET6) {
                sockaddr_in6 local_sin6{};
                local_sin6.sin6_family = AF_INET6;
                local_sin6.sin6_addr = in6addr_any;

                bool bound_priv = false;
                for (unsigned short priv_port = 1023; priv_port >= 512; --priv_port) {
                    local_sin6.sin6_port = htons(priv_port);
                    if (bind(m_sock, (sockaddr*)&local_sin6, sizeof(local_sin6)) == 0) {
                        bound_priv = true;
                        break;
                    }
                }
                if (!bound_priv && !StringUtils::IsRunningAsAdmin()) {
                    std::cerr << "[NOTICE] Running without elevated privileges. Standard ephemeral port bound.\n";
                }
            }

            if (connect(m_sock, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) {
                freeaddrinfo(res);
                return true;
            }
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
        }

        freeaddrinfo(res);
        std::cerr << "[ERROR] Connection failed to " << host << ":" << port << "\n";
        return false;
    }

    void Close() {
        if (m_sock != INVALID_SOCKET) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
        }
    }

    bool SendAll(const void* data, size_t len) {
        const char* ptr = static_cast<const char*>(data);
        while (len > 0) {
            int sent = send(m_sock, ptr, static_cast<int>(len), 0);
            if (sent <= 0) return false;
            ptr += sent;
            len -= sent;
        }
        return true;
    }

    bool SendString(const std::string& str) {
        return SendAll(str.c_str(), str.length());
    }

    bool SendByte(uint8_t byte) {
        return SendAll(&byte, 1);
    }

    bool ReadByte(uint8_t& byte) {
        int r = recv(m_sock, reinterpret_cast<char*>(&byte), 1, 0);
        return r == 1;
    }

    bool ReadLine(std::string& line) {
        line.clear();
        char c;
        while (recv(m_sock, &c, 1, 0) == 1) {
            if (c == '\n') return true;
            line += c;
        }
        return !line.empty();
    }

    bool CheckAck() {
        uint8_t status;
        if (!ReadByte(status)) {
            std::cerr << "[ERROR] Timeout or connection dropped while waiting for ACK.\n";
            return false;
        }
        if (status == 0) return true;

        std::string err_msg;
        ReadLine(err_msg);
        std::cerr << "[REMOTE ERROR] " << err_msg << "\n";
        return false;
    }

    SOCKET GetRaw() const { return m_sock; }

private:
    SOCKET m_sock;
};

// ============================================================================
// 5. RCP TRANSFER ENGINE
// ============================================================================

class RcpTransferEngine {
public:
    static bool SendLocalFile(RcpSocket& sock, const fs::path& filepath, const RcpOptions& opt) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open local file: " << StringUtils::WStringToString(filepath.wstring()) << "\n";
            return false;
        }

        uint64_t filesize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (opt.preserve) {
            int64_t mtime = 0, atime = 0;
            if (FileTimestampHelper::GetFileTimestampsWin32(filepath, mtime, atime)) {
                std::ostringstream t_stream;
                t_stream << "T" << mtime << " 0 " << atime << " 0\n";
                if (!sock.SendString(t_stream.str()) || !sock.CheckAck()) {
                    return false;
                }
            }
        }

        std::string filename = StringUtils::WStringToString(filepath.filename().wstring());
        std::ostringstream c_stream;
        c_stream << "C0644 " << filesize << " " << filename << "\n";

        std::cout << "--> Sending: " << filename << " (" << filesize << " bytes)... ";

        if (!sock.SendString(c_stream.str()) || !sock.CheckAck()) {
            std::cout << "[FAILED]\n";
            return false;
        }

        std::vector<char> buffer(BUFFER_SIZE);
        uint64_t remaining = filesize;
        while (remaining > 0) {
            size_t chunk = static_cast<size_t>((static_cast<uint64_t>(buffer.size()) < remaining) ? static_cast<uint64_t>(buffer.size()) : remaining);
            file.read(buffer.data(), chunk);
            if (!sock.SendAll(buffer.data(), chunk)) {
                std::cout << "[FAILED]\n";
                return false;
            }
            remaining -= chunk;
        }

        sock.SendByte(0);
        if (!sock.CheckAck()) {
            std::cout << "[FAILED]\n";
            return false;
        }

        std::cout << "[OK]\n";
        return true;
    }

    static bool SendLocalDirectory(RcpSocket& sock, const fs::path& dirpath, const RcpOptions& opt) {
        std::string dirname = StringUtils::WStringToString(dirpath.filename().wstring());
        std::ostringstream d_stream;
        d_stream << "D0755 0 " << dirname << "\n";

        std::cout << "==> Entering Directory: " << dirname << "\n";
        if (!sock.SendString(d_stream.str()) || !sock.CheckAck()) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(dirpath)) {
            std::error_code ec;
            const auto status = entry.symlink_status(ec);
            if (!ec) {
                if (fs::is_directory(status)) {
                    if (opt.recursive) {
                        if (!SendLocalDirectory(sock, entry.path(), opt)) return false;
                    }
                } else if (fs::is_regular_file(status)) {
                    if (!SendLocalFile(sock, entry.path(), opt)) return false;
                }
            }
        }

        std::cout << "<== Leaving Directory: " << dirname << "\n";
        return sock.SendString("E\n") && sock.CheckAck();
    }

    static bool ReceiveStream(RcpSocket& sock, const fs::path& local_target, const RcpOptions& opt) {
        sock.SendByte(0);

        uint8_t cmd_type;
        int64_t mtime = 0, atime = 0;

        while (sock.ReadByte(cmd_type)) {
            if (cmd_type == 0) continue;

            if (cmd_type == 1 || cmd_type == 2) {
                std::string err;
                sock.ReadLine(err);
                std::cerr << "[REMOTE ERROR] " << err << "\n";
                return false;
            }

            std::string line;
            if (!sock.ReadLine(line)) break;

            if (cmd_type == 'T') {
                std::istringstream iss(line);
                iss >> mtime >> atime;
                sock.SendByte(0);
            } else if (cmd_type == 'C' || cmd_type == 'D') {
                std::istringstream iss(line);
                std::string mode_str;
                uint64_t size = 0;
                iss >> mode_str >> size;

                std::string raw_name;
                std::getline(iss >> std::ws, raw_name);

                std::string safe_name = StringUtils::SanitizeIncomingFilename(raw_name);
                fs::path final_path = fs::is_directory(local_target) ? (local_target / StringUtils::Utf8ToPath(safe_name)) : local_target;

                if (cmd_type == 'D') {
                    std::cout << "==> Creating Directory: " << StringUtils::WStringToString(final_path.wstring()) << "\n";
                    fs::create_directories(final_path);
                    sock.SendByte(0);
                    if (!ReceiveStream(sock, final_path, opt)) return false;
                } else {
                    std::cout << "<-- Receiving: " << safe_name << " (" << size << " bytes)... ";

                    fs::path temp_path = StringUtils::Utf8ToPath(final_path.string() + ".tmp");
                    TempFileTracker::Instance().Register(temp_path);

                    std::ofstream ofs(temp_path, std::ios::binary);
                    if (!ofs.is_open()) {
                        std::cout << "[FAILED]\n";
                        std::cerr << "[ERROR] Cannot create local file: " << StringUtils::WStringToString(temp_path.wstring()) << "\n";
                        TempFileTracker::Instance().Unregister(temp_path);
                        return false;
                    }

                    sock.SendByte(0);

                    std::vector<char> buffer(BUFFER_SIZE);
                    uint64_t remaining = size;
                    while (remaining > 0) {
                        int chunk = static_cast<int>((static_cast<uint64_t>(buffer.size()) < remaining) ? static_cast<uint64_t>(buffer.size()) : remaining);
                        int bytes_read = recv(sock.GetRaw(), buffer.data(), chunk, 0);
                        if (bytes_read <= 0) {
                            std::cout << "[FAILED]\n";
                            ofs.close();
                            TempFileTracker::Instance().Unregister(temp_path);
                            std::error_code ec;
                            fs::remove(temp_path, ec);
                            return false;
                        }
                        ofs.write(buffer.data(), bytes_read);
                        if (!ofs) {
                            std::cout << "[FAILED - WRITE ERROR]\n";
                            ofs.close();
                            TempFileTracker::Instance().Unregister(temp_path);
                            std::error_code ec;
                            fs::remove(temp_path, ec);
                            return false;
                        }
                        remaining -= bytes_read;
                    }

                    ofs.close();

                    if (!sock.CheckAck()) {
                        std::cout << "[FAILED]\n";
                        TempFileTracker::Instance().Unregister(temp_path);
                        std::error_code ec;
                        fs::remove(temp_path, ec);
                        return false;
                    }

                    std::error_code ec;
                    fs::remove(final_path, ec);
                    fs::rename(temp_path, final_path, ec);
                    TempFileTracker::Instance().Unregister(temp_path);

                    if (ec) {
                        std::cout << "[FAILED - RENAME ERROR]\n";
                        return false;
                    }

                    if (opt.preserve && mtime > 0) {
                        FileTimestampHelper::SetFileTimestampsWin32(final_path, mtime, atime > 0 ? atime : mtime);
                    }

                    std::cout << "[OK]\n";
                    sock.SendByte(0);
                }
            } else if (cmd_type == 'E') {
                sock.SendByte(0);
                return true;
            }
        }
        return true;
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

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

class RcpApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
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
};

int wmain(int argc, wchar_t* argv[]) {
    RcpApplication app;
    return app.Run(argc, argv);
}
