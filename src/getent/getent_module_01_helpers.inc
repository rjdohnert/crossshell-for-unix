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

