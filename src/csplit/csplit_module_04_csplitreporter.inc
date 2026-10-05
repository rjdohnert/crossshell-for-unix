class CsplitReporter {
public:
    static int dispatch(const std::vector<std::pair<std::string, size_t>>& pieces,
                        int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"pieces\":[";
            for (size_t i = 0; i < pieces.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"file\":\"" + pieces[i].first + "\",\"bytes\":" + std::to_string(pieces[i].second) + "}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "file,bytes\n";
            for (const auto& p : pieces) {
                text += "\"" + p.first + "\"," + std::to_string(p.second) + "\n";
            }
        } else if (format == 3) {
            text = "FILE\tBYTES\n--------------------\n";
            for (const auto& p : pieces) {
                text += p.first + "\t" + std::to_string(p.second) + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else if (format != 0) {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. CONTEXT SPLIT ENGINE
// ============================================================================

