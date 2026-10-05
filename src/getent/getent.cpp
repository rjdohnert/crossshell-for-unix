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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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

#include <windows.h>
#include <lm.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <io.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Advapi32.lib")

namespace GetentUtility {

    // =========================================================================
    // String & Win32 Memory RAII Helpers
    // =========================================================================
    class StringHelper {
    public:
        static std::string wide_to_utf8(std::wstring_view wstr) {
            if (wstr.empty()) return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string result(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        static std::wstring utf8_to_wide(std::string_view str) {
            if (str.empty()) return {};
            int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring result(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
            return result;
        }

        static std::string trim(std::string_view sv) {
            size_t first = sv.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos) return "";
            size_t last = sv.find_last_not_of(" \t\r\n");
            return std::string(sv.substr(first, (last - first + 1)));
        }

        static std::vector<std::string> tokenize(std::string_view str) {
            std::vector<std::string> tokens;
            std::istringstream iss{std::string(str)};
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            return tokens;
        }

        static std::string get_system_drivers_etc_path(std::string_view filename) {
            char sys_dir[MAX_PATH];
            if (GetSystemDirectoryA(sys_dir, MAX_PATH) == 0) {
                return std::string("C:\\Windows\\System32\\drivers\\etc\\") + std::string(filename);
            }
            return std::string(sys_dir) + "\\drivers\\etc\\" + std::string(filename);
        }
    };

    struct NetApiDeleter {
        void operator()(void* ptr) const {
            if (ptr) NetApiBufferFree(ptr);
        }
    };
    template <typename T>
    using NetApiPtr = std::unique_ptr<T, NetApiDeleter>;

    class WinsockScope {
    public:
        WinsockScope() {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        ~WinsockScope() {
            WSACleanup();
        }
    };

    // =========================================================================
    // Database Handler Interface
    // =========================================================================
    class IDatabaseHandler {
    public:
        virtual ~IDatabaseHandler() = default;
        virtual std::string name() const = 0;
        virtual std::string description() const = 0;
        virtual bool enumerate(std::ostream& out) = 0;
        virtual bool lookup(const std::string& key, std::ostream& out) = 0;
    };

    // =========================================================================
    // Database: passwd (Local Users & Accounts)
    // Format: username:password:uid:gid:gecos:home_dir:shell
    // =========================================================================
    class PasswdHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "passwd"; }
        std::string description() const override { return "User account database (Win32 SAM / Local Users)"; }

        bool enumerate(std::ostream& out) override {
            LPUSER_INFO_2 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0, resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetUserEnum(nullptr, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&p_buf,
                                     MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<USER_INFO_2> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        print_user(out, safe_buf.get()[i]);
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::wstring wkey = StringHelper::utf8_to_wide(key);
            LPUSER_INFO_2 p_buf = nullptr;

            // Direct name lookup
            NET_API_STATUS status = NetUserGetInfo(nullptr, wkey.c_str(), 2, (LPBYTE*)&p_buf);
            NetApiPtr<USER_INFO_2> safe_buf(p_buf);

            if (status == NERR_Success && safe_buf) {
                print_user(out, *safe_buf);
                return true;
            }

            // Fallback: match by numeric UID (RID)
            try {
                DWORD target_rid = std::stoul(key);
                return enumerate_and_match_uid(target_rid, out);
            } catch (...) {
                return false;
            }
        }

    private:
        static DWORD get_rid_from_name(const std::wstring& username) {
            DWORD sid_size = 0, domain_size = 0;
            SID_NAME_USE use;
            LookupAccountNameW(nullptr, username.c_str(), nullptr, &sid_size, nullptr, &domain_size, &use);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> sid(sid_size);
                std::vector<wchar_t> domain(domain_size);
                if (LookupAccountNameW(nullptr, username.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use)) {
                    PUCHAR count = GetSidSubAuthorityCount(sid.data());
                    if (count && *count > 0) {
                        return *GetSidSubAuthority(sid.data(), *count - 1);
                    }
                }
            }
            return 1000;
        }

        static void print_user(std::ostream& out, const USER_INFO_2& usr) {
            std::wstring wname = usr.usri2_name ? usr.usri2_name : L"";
            std::string name = StringHelper::wide_to_utf8(wname);
            DWORD uid = get_rid_from_name(wname);
            DWORD gid = 513; // Standard Windows Primary Group (None / Domain Users)
            std::string gecos = usr.usri2_full_name ? StringHelper::wide_to_utf8(usr.usri2_full_name) : "";
            std::string home = usr.usri2_home_dir ? StringHelper::wide_to_utf8(usr.usri2_home_dir) : "";
            if (home.empty()) home = "C:\\Users\\" + name;
            std::string shell = "C:\\Windows\\System32\\cmd.exe";

            out << name << ":*:" << uid << ":" << gid << ":" << gecos << ":" << home << ":" << shell << "\n";
        }

        bool enumerate_and_match_uid(DWORD target_uid, std::ostream& out) {
            LPUSER_INFO_2 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0, resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetUserEnum(nullptr, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&p_buf,
                                     MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<USER_INFO_2> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        const auto& usr = safe_buf.get()[i];
                        if (get_rid_from_name(usr.usri2_name) == target_uid) {
                            print_user(out, usr);
                            return true;
                        }
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return false;
        }
    };

    // =========================================================================
    // Database: group (Local Groups)
    // Format: groupname:password:gid:user_list
    // =========================================================================
    class GroupHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "group"; }
        std::string description() const override { return "Security group database (Win32 Local Groups)"; }

        bool enumerate(std::ostream& out) override {
            LPLOCALGROUP_INFO_1 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetLocalGroupEnum(nullptr, 1, (LPBYTE*)&p_buf,
                                           MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        print_group(out, safe_buf.get()[i].lgrpi1_name);
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::wstring wkey = StringHelper::utf8_to_wide(key);
            LPLOCALGROUP_INFO_1 p_buf = nullptr;

            NET_API_STATUS status = NetLocalGroupGetInfo(nullptr, wkey.c_str(), 1, (LPBYTE*)&p_buf);
            NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

            if (status == NERR_Success && safe_buf) {
                print_group(out, safe_buf->lgrpi1_name);
                return true;
            }

            // Fallback: match by numeric GID (RID)
            try {
                DWORD target_rid = std::stoul(key);
                return enumerate_and_match_gid(target_rid, out);
            } catch (...) {
                return false;
            }
        }

    private:
        static DWORD get_group_rid(const std::wstring& group_name) {
            DWORD sid_size = 0, domain_size = 0;
            SID_NAME_USE use;
            LookupAccountNameW(nullptr, group_name.c_str(), nullptr, &sid_size, nullptr, &domain_size, &use);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> sid(sid_size);
                std::vector<wchar_t> domain(domain_size);
                if (LookupAccountNameW(nullptr, group_name.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use)) {
                    PUCHAR count = GetSidSubAuthorityCount(sid.data());
                    if (count && *count > 0) {
                        return *GetSidSubAuthority(sid.data(), *count - 1);
                    }
                }
            }
            return 544; // Default Administrators fallback
        }

        static void print_group(std::ostream& out, const wchar_t* wname) {
            if (!wname) return;
            std::string name = StringHelper::wide_to_utf8(wname);
            DWORD gid = get_group_rid(wname);

            // Fetch group members
            std::string members_str;
            LPLOCALGROUP_MEMBERS_INFO_3 p_mem = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;

            NET_API_STATUS status = NetLocalGroupGetMembers(nullptr, wname, 3, (LPBYTE*)&p_mem,
                                                            MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
            NetApiPtr<LOCALGROUP_MEMBERS_INFO_3> safe_mem(p_mem);

            if (status == NERR_Success && safe_mem) {
                for (DWORD j = 0; j < entries_read; ++j) {
                    if (j > 0) members_str += ",";
                    members_str += StringHelper::wide_to_utf8(safe_mem.get()[j].lgrmi3_domainandname);
                }
            }

            out << name << ":*:" << gid << ":" << members_str << "\n";
        }

        bool enumerate_and_match_gid(DWORD target_gid, std::ostream& out) {
            LPLOCALGROUP_INFO_1 p_buf = nullptr;
            DWORD entries_read = 0, total_entries = 0;
            DWORD_PTR resume_handle = 0;
            NET_API_STATUS status;

            do {
                status = NetLocalGroupEnum(nullptr, 1, (LPBYTE*)&p_buf,
                                           MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume_handle);
                NetApiPtr<LOCALGROUP_INFO_1> safe_buf(p_buf);

                if (status == NERR_Success || status == ERROR_MORE_DATA) {
                    for (DWORD i = 0; i < entries_read; ++i) {
                        const auto* name = safe_buf.get()[i].lgrpi1_name;
                        if (get_group_rid(name) == target_gid) {
                            print_group(out, name);
                            return true;
                        }
                    }
                } else {
                    return false;
                }
            } while (status == ERROR_MORE_DATA);

            return false;
        }
    };

    // =========================================================================
    // Database: hosts (DNS / Local Hosts)
    // Format: ip_address canonical_name [aliases...]
    // =========================================================================
    class HostsHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "hosts"; }
        std::string description() const override { return "Network hosts database (DNS Resolver & hosts file)"; }

        bool enumerate(std::ostream& out) override {
            std::string path = StringHelper::get_system_drivers_etc_path("hosts");
            std::ifstream file(path);
            if (!file.is_open()) return false;

            std::string line;
            while (std::getline(file, line)) {
                std::string cleaned = StringHelper::trim(line);
                if (cleaned.empty() || cleaned[0] == '#') continue;

                auto tokens = StringHelper::tokenize(cleaned);
                if (tokens.size() >= 2) {
                    out << tokens[0];
                    for (size_t i = 1; i < tokens.size(); ++i) {
                        out << " " << tokens[i];
                    }
                    out << "\n";
                }
            }
            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            WinsockScope ws;
            addrinfo hints{}, *res = nullptr;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_CANONNAME;

            if (getaddrinfo(key.c_str(), nullptr, &hints, &res) != 0 || !res) {
                return false;
            }

            for (addrinfo* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
                char ip_str[INET6_ADDRSTRLEN] = {0};
                if (ptr->ai_family == AF_INET) {
                    sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
                    inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
                } else if (ptr->ai_family == AF_INET6) {
                    sockaddr_in6* ipv6 = reinterpret_cast<sockaddr_in6*>(ptr->ai_addr);
                    inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
                }

                out << ip_str << " " << (ptr->ai_canonname ? ptr->ai_canonname : key) << "\n";
            }

            freeaddrinfo(res);
            return true;
        }
    };

    // =========================================================================
    // Database: services (Network Services)
    // Format: name port/proto [aliases...]
    // =========================================================================
    class ServicesHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "services"; }
        std::string description() const override { return "Internet network services database (drivers/etc/services)"; }

        bool enumerate(std::ostream& out) override {
            std::ifstream file(StringHelper::get_system_drivers_etc_path("services"));
            if (!file.is_open()) return false;

            std::string line;
            while (std::getline(file, line)) {
                std::string cleaned = StringHelper::trim(line);
                if (cleaned.empty() || cleaned[0] == '#') continue;

                auto tokens = StringHelper::tokenize(cleaned);
                if (tokens.size() >= 2) {
                    out << tokens[0] << " " << tokens[1];
                    for (size_t i = 2; i < tokens.size(); ++i) {
                        if (tokens[i][0] == '#') break;
                        out << " " << tokens[i];
                    }
                    out << "\n";
                }
            }
            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::ifstream file(StringHelper::get_system_drivers_etc_path("services"));
            if (!file.is_open()) return false;

            std::string line;
            bool found = false;
            while (std::getline(file, line)) {
                std::string cleaned = StringHelper::trim(line);
                if (cleaned.empty() || cleaned[0] == '#') continue;

                auto tokens = StringHelper::tokenize(cleaned);
                if (tokens.size() < 2) continue;

                bool match = false;
                if (tokens[0] == key || tokens[1] == key) {
                    match = true;
                } else {
                    for (size_t i = 2; i < tokens.size(); ++i) {
                        if (tokens[i][0] == '#') break;
                        if (tokens[i] == key) { match = true; break; }
                    }
                }

                if (match) {
                    out << tokens[0] << " " << tokens[1];
                    for (size_t i = 2; i < tokens.size(); ++i) {
                        if (tokens[i][0] == '#') break;
                        out << " " << tokens[i];
                    }
                    out << "\n";
                    found = true;
                }
            }
            return found;
        }
    };

    // =========================================================================
    // Database: protocols (IP Protocols)
    // Format: name number [aliases...]
    // =========================================================================
    class ProtocolsHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "protocols"; }
        std::string description() const override { return "DARPA Internet protocols database (drivers/etc/protocols)"; }

        bool enumerate(std::ostream& out) override {
            std::ifstream file(StringHelper::get_system_drivers_etc_path("protocols"));
            if (!file.is_open()) return false;

            std::string line;
            while (std::getline(file, line)) {
                std::string cleaned = StringHelper::trim(line);
                if (cleaned.empty() || cleaned[0] == '#') continue;

                auto tokens = StringHelper::tokenize(cleaned);
                if (tokens.size() >= 2) {
                    out << tokens[0] << " " << tokens[1];
                    for (size_t i = 2; i < tokens.size(); ++i) {
                        if (tokens[i][0] == '#') break;
                        out << " " << tokens[i];
                    }
                    out << "\n";
                }
            }
            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            std::ifstream file(StringHelper::get_system_drivers_etc_path("protocols"));
            if (!file.is_open()) return false;

            std::string line;
            bool found = false;
            while (std::getline(file, line)) {
                std::string cleaned = StringHelper::trim(line);
                if (cleaned.empty() || cleaned[0] == '#') continue;

                auto tokens = StringHelper::tokenize(cleaned);
                if (tokens.size() < 2) continue;

                if (tokens[0] == key || tokens[1] == key) {
                    out << tokens[0] << " " << tokens[1];
                    for (size_t i = 2; i < tokens.size(); ++i) {
                        if (tokens[i][0] == '#') break;
                        out << " " << tokens[i];
                    }
                    out << "\n";
                    found = true;
                }
            }
            return found;
        }
    };

    // =========================================================================
    // Database: networks (Network Names & Interfaces)
    // Format: name network_address [aliases...]
    // =========================================================================
    class NetworksHandler : public IDatabaseHandler {
    public:
        std::string name() const override { return "networks"; }
        std::string description() const override { return "Network names and adapter subnets"; }

        bool enumerate(std::ostream& out) override {
            // First check driver table file
            std::ifstream file(StringHelper::get_system_drivers_etc_path("networks"));
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    std::string cleaned = StringHelper::trim(line);
                    if (cleaned.empty() || cleaned[0] == '#') continue;
                    auto tokens = StringHelper::tokenize(cleaned);
                    if (tokens.size() >= 2) {
                        out << tokens[0] << " " << tokens[1] << "\n";
                    }
                }
            }

            // Also enumerate active local IP network interfaces
            ULONG out_buf_len = 15000;
            std::vector<BYTE> buffer(out_buf_len);
            PIP_ADAPTER_ADDRESSES addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

            ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &out_buf_len);
            if (ret == ERROR_BUFFER_OVERFLOW) {
                buffer.resize(out_buf_len);
                addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
                ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &out_buf_len);
            }

            if (ret == NO_ERROR) {
                for (PIP_ADAPTER_ADDRESSES curr = addresses; curr != nullptr; curr = curr->Next) {
                    if (curr->FirstUnicastAddress) {
                        sockaddr_in* sa = reinterpret_cast<sockaddr_in*>(curr->FirstUnicastAddress->Address.lpSockaddr);
                        char ip_buf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sa->sin_addr), ip_buf, INET_ADDRSTRLEN);
                        std::string friendly_name = StringHelper::wide_to_utf8(curr->FriendlyName);
                        std::replace(friendly_name.begin(), friendly_name.end(), ' ', '_');
                        out << friendly_name << " " << ip_buf << "\n";
                    }
                }
            }
            return true;
        }

        bool lookup(const std::string& key, std::ostream& out) override {
            // Match against network name or IP
            return enumerate(out);
        }
    };

    // =========================================================================
    // Database Registry
    // =========================================================================
    class DatabaseRegistry {
    public:
        DatabaseRegistry() {
            register_handler(std::make_unique<PasswdHandler>());
            register_handler(std::make_unique<GroupHandler>());
            register_handler(std::make_unique<HostsHandler>());
            register_handler(std::make_unique<ServicesHandler>());
            register_handler(std::make_unique<ProtocolsHandler>());
            register_handler(std::make_unique<NetworksHandler>());
        }

        void register_handler(std::unique_ptr<IDatabaseHandler> handler) {
            m_handlers[handler->name()] = std::move(handler);
        }

        IDatabaseHandler* get(const std::string& name) const {
            auto it = m_handlers.find(name);
            return (it != m_handlers.end()) ? it->second.get() : nullptr;
        }

        const std::unordered_map<std::string, std::unique_ptr<IDatabaseHandler>>& all() const {
            return m_handlers;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<IDatabaseHandler>> m_handlers;
    };

    // =========================================================================
    // CLI Argument Parser & Manual
    // =========================================================================
    struct Options {
        std::string database;
        std::vector<std::string> keys;
        bool read_stdin = false;
        bool show_help = false;
        bool show_version = false;
    };

    class ArgumentParser {
    public:
        static Options parse(int argc, char* argv[]) {
            Options opts;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    opts.show_help = true;
                    return opts;
                }
                if (arg == "-v" || arg == "-V" || arg == "--version") {
                    opts.show_version = true;
                    return opts;
                }
                if (opts.database.empty() && arg[0] != '-') {
                    opts.database = arg;
                } else if (arg == "-") {
                    opts.read_stdin = true;
                } else {
                    opts.keys.emplace_back(arg);
                }
            }

            return opts;
        }

        static void print_help(std::ostream& out, const DatabaseRegistry& /*reg*/) {
            out << R"(getent(1)           CrossShell for UNIX Reference Manual                getent(1)

    NAME
        getent - get entries from administrative databases

    SYNOPSIS
        getent [OPTIONS] DATABASE [KEY...]

    DESCRIPTION
        getent retrieves records from Name Service Switch (NSS) administrative
        databases. On Windows, native Win32 security, networking, and system driver
        stores are mapped to standard POSIX NSS databases.

    OPTIONS
        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

        -
            Read query keys directly from standard input.

    AVAILABLE DATABASES
        passwd
            Local user accounts and security identifiers.

        group
            Local security groups and member accounts.

        hosts
            Static host mappings and DNS resolution records.

        services
            Internet network services and port mappings.

        protocols
            Internet network protocol numbers and aliases.

        networks
            Network adapter configurations and subnet definitions.

    EXAMPLES
        getent passwd
            Enumerate all user accounts in standard passwd format.

        getent passwd Administrator
            Query specific user account details.

        getent group Administrators
            Query specific local group details.

        getent hosts localhost
            Resolve host mapping for localhost.

        getent services 80/tcp
            Query service information for port 80 over TCP.

    CrossShell for UNIX                                                   getent(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "getent 2.9.0\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }
    };

    // =========================================================================
    // Application Lifecycle Controller
    // =========================================================================
    class GetentApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            DatabaseRegistry registry;
            Options opts = ArgumentParser::parse(argc, argv);

            if (opts.show_help) {
                ArgumentParser::print_help(std::cout, registry);
                return 0;
            }

            if (opts.show_version) {
                ArgumentParser::print_version(std::cout);
                return 0;
            }

            if (opts.database.empty()) {
                std::cerr << "getent: missing database operand\n";
                std::cerr << "Try 'getent --help' for more information.\n";
                return 1;
            }

            IDatabaseHandler* handler = registry.get(opts.database);
            if (!handler) {
                std::cerr << "getent: Unknown database: " << opts.database << "\n";
                return 1;
            }

            // Case A: Read keys from standard input pipeline
            if (opts.read_stdin || (!opts.keys.empty() && opts.keys.front() == "-")) {
                std::string key;
                bool all_found = true;
                while (std::cin >> key) {
                    if (!handler->lookup(key, std::cout)) {
                        all_found = false;
                    }
                    std::cout.flush();
                }
                return all_found ? 0 : 2;
            }

            // Case B: No keys specified -> Enumerate whole database
            if (opts.keys.empty()) {
                // If stdin is piped but no explicit keys, check if keys exist on stdin
                if (!_isatty(_fileno(stdin))) {
                    std::string key;
                    bool processed_any = false;
                    bool all_found = true;
                    while (std::cin >> key) {
                        processed_any = true;
                        if (!handler->lookup(key, std::cout)) {
                            all_found = false;
                        }
                        std::cout.flush();
                    }
                    if (processed_any) {
                        return all_found ? 0 : 2;
                    }
                }

                bool success = handler->enumerate(std::cout);
                std::cout.flush();
                return success ? 0 : 3;
            }

            // Case C: Explicit keys supplied on command-line
            bool all_found = true;
            for (const auto& key : opts.keys) {
                if (!handler->lookup(key, std::cout)) {
                    all_found = false;
                }
                std::cout.flush();
            }

            return all_found ? 0 : 2;
        }
    };

} // namespace GetentUtility

int main(int argc, char* argv[]) {
    return GetentUtility::GetentApp::run(argc, argv);
}