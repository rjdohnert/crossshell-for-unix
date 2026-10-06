#include "output_reporter.hpp"

OutputReporter::OutputReporter(int fmt, std::string pipeCmd)
        : format(fmt), pipeCommand(std::move(pipeCmd)) {}

int OutputReporter::finish(const std::string& prefix, uint64_t files) const {
        if (format == 0 && pipeCommand.empty()) return 0;

        std::string text;
        if (format == 1) {
            text = "{\"status\":\"success\",\"prefix\":\"" + prefix + "\",\"files\":" + std::to_string(files) + "}\n";
        } else if (format == 2) {
            text = "status,prefix,files\nsuccess," + prefix + "," + std::to_string(files) + "\n";
        } else if (format == 3) {
            text = "STATUS\tPREFIX\tFILES\nsuccess\t" + prefix + "\t" + std::to_string(files) + "\n";
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
