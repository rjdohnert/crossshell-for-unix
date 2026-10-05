// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class CalReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"calendar\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "calendar\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "CALENDAR\n--------\n" + content + "\n";
        } else {
            text = content;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

