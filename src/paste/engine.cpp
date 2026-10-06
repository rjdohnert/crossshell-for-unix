#include "engine.hpp"

// ============================================================================
// DelimiterParser & StreamReader Implementation
// ============================================================================

std::vector<std::string> DelimiterParser::Parse(const std::string& list) {
    std::vector<std::string> delims;
    if (list.empty()) {
        return delims;
    }
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == '\\') {
            if (i + 1 < list.size()) {
                char next = list[i + 1];
                switch (next) {
                    case 't': delims.push_back("\t"); break;
                    case 'n': delims.push_back("\n"); break;
                    case 'r': delims.push_back("\r"); break;
                    case 'f': delims.push_back("\f"); break;
                    case '\\': delims.push_back("\\"); break;
                    case '0': delims.push_back(""); break;
                    default: delims.push_back(std::string(1, next)); break;
                }
                ++i;
            } else {
                delims.push_back("\\");
            }
        } else {
            delims.push_back(std::string(1, list[i]));
        }
    }
    return delims;
}

bool StreamReader::GetNextLine(InputStream& stream, std::string& line) {
    if (stream.eof) {
        line = "";
        return false;
    }
    std::istream& in = stream.is_stdin ? std::cin : stream.file;
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    } else {
        stream.eof = true;
        line = "";
        return false;
    }
}

// ============================================================================
// PasteEngine Implementation
// ============================================================================

PasteEngine::PasteEngine(std::vector<std::string> delimiters)
    : m_delimiters(std::move(delimiters)) {
    if (m_delimiters.empty()) {
        m_delimiters.push_back("");
    }
}

void PasteEngine::ExecuteSerial(const std::vector<std::string>& files) const {
    for (const auto& file : files) {
        InputStream stream;
        if (file == "-") {
            stream.is_stdin = true;
        } else {
            stream.file.open(file, std::ios::binary);
        }

        std::string line;
        bool first = true;
        size_t delim_idx = 0;

        while (StreamReader::GetNextLine(stream, line)) {
            if (!first) {
                std::cout << m_delimiters[delim_idx % m_delimiters.size()];
                delim_idx++;
            }
            std::cout << line;
            first = false;
        }
        if (!first) {
            std::cout << "\n";
        }
    }
}

void PasteEngine::ExecuteParallel(const std::vector<std::string>& files) const {
    std::vector<InputStream> streams;
    for (const auto& file : files) {
        InputStream stream;
        if (file == "-") {
            stream.is_stdin = true;
        } else {
            stream.file.open(file, std::ios::binary);
        }
        streams.push_back(std::move(stream));
    }

    size_t num_streams = streams.size();
    while (true) {
        std::vector<std::string> current_lines(num_streams);
        std::vector<bool> success_flags(num_streams);
        bool any_active = false;

        for (size_t i = 0; i < num_streams; ++i) {
            if (!streams[i].eof) {
                any_active = true;
                std::string line;
                bool ok = StreamReader::GetNextLine(streams[i], line);
                current_lines[i] = line;
                success_flags[i] = ok;
            } else {
                current_lines[i] = "";
                success_flags[i] = false;
            }
        }

        if (!any_active) {
            break;
        }

        bool any_success = false;
        for (size_t i = 0; i < num_streams; ++i) {
            if (success_flags[i]) {
                any_success = true;
                break;
            }
        }
        if (!any_success) {
            break;
        }

        for (size_t i = 0; i < num_streams; ++i) {
            std::cout << current_lines[i];
            if (i < num_streams - 1) {
                std::cout << m_delimiters[i % m_delimiters.size()];
            }
        }
        std::cout << "\n";
    }
}
