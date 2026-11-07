#include "eve_app.h"
#include "eve_editor.h"
#include "eve_pipe.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string UpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

void PrintUsage() {
    std::cout << "Usage: eve [--read-only] [--json|--csv|--table] [--pipe COMMAND] [--] [FILE...]\n"
                 "       eve --help\n";
}

std::string FormatSummary(int format, EditorRunStatus status, size_t file_count) {
    const char* status_text = status == EditorRunStatus::Completed ? "completed" :
        status == EditorRunStatus::Interrupted ? "interrupted" : "failed";
    if (format == 1) {
        return std::string("{\"status\":\"") + status_text + "\",\"files\":" +
            std::to_string(file_count) + "}\n";
    }
    if (format == 2) {
        return std::string("status,files\n") + status_text + "," +
            std::to_string(file_count) + "\n";
    }
    return std::string("STATUS\tFILES\n") + status_text + "\t" +
        std::to_string(file_count) + "\n";
}

}

int RunEveApplication(int argc, char* argv[]) {
    std::vector<std::string> files;
    bool read_only = false;
    int format = 0;
    std::string pipeCommand;
    bool has_pipe = false;
    bool end_options = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            std::cerr << "eve: invalid null argument\n";
            return 2;
        }
        std::string arg = argv[i];
        const std::string upper_arg = UpperAscii(arg);
        if (!end_options && (upper_arg == "/HELP" || upper_arg == "/?" ||
            upper_arg == "--HELP" || upper_arg == "-H")) {
            PrintUsage();
            return 0;
        }
        if (!end_options && arg == "--") {
            end_options = true;
            continue;
        }
        if (!end_options && (upper_arg == "/JSON" || upper_arg == "--JSON")) {
            if (format != 0 && format != 1) {
                std::cerr << "eve: choose only one output format\n";
                return 2;
            }
            format = 1;
            continue;
        }
        if (!end_options && (upper_arg == "/CSV" || upper_arg == "--CSV")) {
            if (format != 0 && format != 2) {
                std::cerr << "eve: choose only one output format\n";
                return 2;
            }
            format = 2;
            continue;
        }
        if (!end_options && (upper_arg == "/TABLE" || upper_arg == "--TABLE")) {
            if (format != 0 && format != 3) {
                std::cerr << "eve: choose only one output format\n";
                return 2;
            }
            format = 3;
            continue;
        }
        if (!end_options && (upper_arg == "/PIPE" || upper_arg == "--PIPE")) {
            if (has_pipe || i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == '\0') {
                std::cerr << "eve: --pipe requires one non-empty command and may be specified once\n";
                return 2;
            }
            pipeCommand = argv[++i];
            has_pipe = true;
            continue;
        }
        if (!end_options && (upper_arg == "/READ_ONLY" || upper_arg == "/NOWRITE" ||
            upper_arg == "--READ-ONLY" || upper_arg == "-R")) {
            read_only = true;
            continue;
        }
        if (!end_options && !arg.empty() && arg[0] == '-') {
            std::cerr << "eve: unknown option: " << arg << '\n';
            return 2;
        }
        if (arg.empty()) {
            std::cerr << "eve: file name cannot be empty\n";
            return 2;
        }
        files.push_back(std::move(arg));
    }

    std::string error;
    const EditorRunStatus status = RunEditor(files, read_only, error);
    if (status == EditorRunStatus::Failed) {
        std::cerr << "eve: " << (error.empty() ? "editor failed" : error) << '\n';
        return 1;
    }

    if (format != 0 || has_pipe) {
        const std::string text = FormatSummary(format == 0 ? 3 : format, status, files.size());
        if (has_pipe) {
            if (!WriteCommandInput(pipeCommand, text, error)) {
                std::cerr << "eve: " << (error.empty() ? "pipe command failed" : error) << '\n';
                return 1;
            }
        } else {
            std::cout << text;
            std::cout.flush();
            if (!std::cout) {
                std::cerr << "eve: unable to write structured output\n";
                return 1;
            }
        }
    }
    return status == EditorRunStatus::Interrupted ? 130 : 0;
}
