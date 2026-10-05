// ============================================================================
// 3. OUTPUT REPORTER
// ============================================================================

class HostidReporter {
public:
    static void Emit(const HostidOptions& opts, uint32_t hostId) {
        std::ostringstream output;
        output << std::setw(8) << std::setfill('0') << std::hex << hostId;
        std::string hexStr = output.str();

        std::string text;
        if (opts.format == OutputFormat::Json) {
            text = "{\"hostid\":\"" + hexStr + "\"}\n";
        } else if (opts.format == OutputFormat::Csv) {
            text = "hostid\n" + hexStr + "\n";
        } else if (opts.format == OutputFormat::Table) {
            text = "HOSTID\n------\n" + hexStr + "\n";
        } else {
            text = hexStr + "\n";
        }

        if (!opts.pipeCommand.empty()) {
            FILE* pipe = _popen(opts.pipeCommand.c_str(), "w");
            if (pipe) {
                fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::cout << text;
        }
    }
};

