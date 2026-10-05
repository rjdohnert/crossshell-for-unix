// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class IconvReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand, std::ostream& directOut) {
        if (format != 0 || !pipeCommand.empty()) {
            std::string text;
            if (format == 1) {
                text = "{\"data\":\"" + content + "\"}\n";
            } else if (format == 2) {
                text = "data\n\"" + content + "\"\n";
            } else if (format == 3) {
                text = "DATA\n----\n" + content + "\n";
            } else {
                text = content;
            }

            if (!pipeCommand.empty()) {
                FILE* pipe = _popen(pipeCommand.c_str(), "w");
                if (!pipe) return 1;
                std::fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            } else {
                directOut << text;
            }
        } else {
            directOut << content;
        }
        return 0;
    }
};

