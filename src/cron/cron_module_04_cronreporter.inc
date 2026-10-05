class CronReporter {
public:
    static int dispatchStatus(size_t jobCount, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"status\":\"running\",\"jobs\":" + std::to_string(jobCount) + "}\n";
        } else if (format == 2) {
            text = "status,jobs\nrunning," + std::to_string(jobCount) + "\n";
        } else if (format == 3) {
            text = "STATUS\tJOBS\nrunning\t" + std::to_string(jobCount) + "\n";
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
// 4. CRONTAB PARSER & SCHEDULER ENGINE
// ============================================================================

