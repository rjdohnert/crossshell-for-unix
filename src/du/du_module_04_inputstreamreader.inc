    // =========================================================================
    // Pipe / Stream Path Ingestion
    // =========================================================================
    class InputStreamReader {
    public:
        static std::vector<std::string> read_paths_from_stream(std::istream& in, char delimiter) {
            std::vector<std::string> paths;
            std::string path;
            while (std::getline(in, path, delimiter)) {
                if (delimiter == '\n' && !path.empty() && path.back() == '\r') {
                    path.pop_back();
                }
                if (!path.empty()) {
                    paths.push_back(path);
                }
            }
            return paths;
        }
    };
