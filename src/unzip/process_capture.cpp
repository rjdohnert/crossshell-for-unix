#include "process_capture.hpp"

std::string run_capture(const std::string& command, int& exitCode) {
    std::string out;

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        exitCode = -1;
        return out;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }

    exitCode = _pclose(pipe);
    return out;
}
