// ============================================================================
// 2. CONFIGURATION PARSER & STRING UTILS
// ============================================================================

class InetdConfigParser {
public:
    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::vector<std::string> SplitConfigLine(const std::string& str) {
        std::vector<std::string> tokens;
        std::string current;
        bool in_quotes = false;
        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }

    static bool Parse(const std::string& filepath, std::vector<ServiceConfig>& services) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            InetdLogger::Instance().Log(LOG_ERROR, "Unable to open configuration file: " + filepath);
            return false;
        }

        std::string line;
        size_t line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            std::string trimmed = Trim(line);
            if (trimmed.empty()) continue;

            auto tokens = SplitConfigLine(trimmed);
            if (tokens.size() < 6) {
                InetdLogger::Instance().Log(LOG_WARN, "Malformed line " + std::to_string(line_num) + " in " + filepath);
                continue;
            }

            ServiceConfig cfg;
            cfg.name      = tokens[0];
            cfg.sock_type = tokens[1];
            cfg.protocol  = tokens[2];
            cfg.wait_flag = (tokens[3] == "wait");
            cfg.user      = tokens[4];
            cfg.program   = tokens[5];

            for (size_t i = 6; i < tokens.size(); ++i) {
                if (i > 6) cfg.args += " ";
                cfg.args += tokens[i];
            }

            if (cfg.sock_type == "dgram" && !cfg.wait_flag) {
                InetdLogger::Instance().Log(LOG_WARN, "UDP service '" + cfg.name + "' is configured as nowait, forcing to wait mode to avoid thread storms.");
                cfg.wait_flag = true;
            }

            cfg.port_str = cfg.name;
            services.push_back(cfg);

            InetdLogger::Instance().Log(LOG_INFO, "Registered service '" + cfg.name + "' (" + cfg.protocol + ") -> " + cfg.program);
        }

        return !services.empty();
    }
};

