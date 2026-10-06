#include "pipe_stream_handler.hpp"

std::vector<std::string> PipeStreamHandler::ReadTargetsFromStream(bool nullDelimited) {
        std::vector<std::string> paths;
        _setmode(_fileno(stdin), _O_BINARY);

        if (nullDelimited) {
            std::string current;
            char ch;
            while (std::cin.get(ch)) {
                if (ch == '\0') {
                    if (!current.empty()) {
                        paths.push_back(current);
                        current.clear();
                    }
                } else {
                    current.push_back(ch);
                }
            }
            if (!current.empty()) paths.push_back(current);
        } else {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) {
                    paths.push_back(line);
                }
            }
        }
        return paths;
    }
