#include "input_tokenizer.hpp"
#include "xargs_options.hpp"

std::vector<std::string> InputTokenizer::readAll(std::istream& in, const XargsOptions& opts) {
        std::vector<std::string> tokens;

        if (opts.mode == DelimitMode::NULL_CHAR || opts.mode == DelimitMode::DELIMITER) {
            char sep = (opts.mode == DelimitMode::NULL_CHAR) ? '\0' : opts.delimiter;
            std::string tok;
            while (std::getline(in, tok, sep)) {
                if (!tok.empty()) tokens.push_back(tok);
            }
        } else if (opts.mode == DelimitMode::NEWLINE) {
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) tokens.push_back(line);
            }
        } else { // WHITESPACE
            std::string tok;
            while (in >> tok) {
                tokens.push_back(tok);
            }
        }

        return tokens;
    }
