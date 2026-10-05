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

