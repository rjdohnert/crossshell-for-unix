#include "pattern_expander.hpp"

std::vector<uint8_t> PatternExpander::expand(const std::string& str, size_t targetLen ) {
        std::vector<uint8_t> result;
        size_t i = 0;
        size_t len = str.length();

        while (i < len) {
            // POSIX Character Classes: [:class:]
            if (i + 2 < len && str[i] == '[' && str[i + 1] == ':') {
                size_t endCls = str.find(":]", i + 2);
                if (endCls != std::string::npos) {
                    std::string cls = str.substr(i + 2, endCls - (i + 2));
                    bool matched = true;

                    for (int c = 0; c < 256; ++c) {
                        unsigned char uc = static_cast<unsigned char>(c);
                        if (cls == "alnum"  && std::isalnum(uc))  result.push_back(uc);
                        else if (cls == "alpha"  && std::isalpha(uc))  result.push_back(uc);
                        else if (cls == "cntrl"  && std::iscntrl(uc))  result.push_back(uc);
                        else if (cls == "digit"  && std::isdigit(uc))  result.push_back(uc);
                        else if (cls == "graph"  && std::isgraph(uc))  result.push_back(uc);
                        else if (cls == "lower"  && std::islower(uc))  result.push_back(uc);
                        else if (cls == "print"  && std::isprint(uc))  result.push_back(uc);
                        else if (cls == "punct"  && std::ispunct(uc))  result.push_back(uc);
                        else if (cls == "space"  && std::isspace(uc))  result.push_back(uc);
                        else if (cls == "upper"  && std::isupper(uc))  result.push_back(uc);
                        else if (cls == "xdigit" && std::isxdigit(uc)) result.push_back(uc);
                        else if (c == 255 && cls != "alnum" && cls != "alpha" && cls != "cntrl" &&
                                 cls != "digit" && cls != "graph" && cls != "lower" &&
                                 cls != "print" && cls != "punct" && cls != "space" &&
                                 cls != "upper" && cls != "xdigit") {
                            matched = false;
                        }
                    }

                    if (matched) {
                        i = endCls + 2;
                        continue;
                    }
                }
            }

            // Character Repetition in string2: [c*n] or [c*]
            if (i + 2 < len && str[i] == '[' && str[i + 2] == '*') {
                size_t endRep = str.find(']', i + 3);
                if (endRep != std::string::npos) {
                    uint8_t repChar = static_cast<uint8_t>(str[i + 1]);
                    std::string countStr = str.substr(i + 3, endRep - (i + 3));
                    size_t count = 0;

                    if (countStr.empty()) {
                        if (targetLen > result.size()) {
                            count = targetLen - result.size();
                        }
                    } else {
                        if (countStr[0] == '0') {
                            count = std::strtoul(countStr.c_str(), nullptr, 8);
                        } else {
                            count = std::strtoul(countStr.c_str(), nullptr, 10);
                        }
                    }

                    for (size_t k = 0; k < count; ++k) {
                        result.push_back(repChar);
                    }
                    i = endRep + 1;
                    continue;
                }
            }

            // Parse character or escape sequence
            auto parseChar = [&](size_t& idx) -> uint8_t {
                if (str[idx] == '\\' && idx + 1 < len) {
                    idx++;
                    char esc = str[idx++];
                    switch (esc) {
                        case 'a': return '\a';
                        case 'b': return '\b';
                        case 'f': return '\f';
                        case 'n': return '\n';
                        case 'r': return '\r';
                        case 't': return '\t';
                        case 'v': return '\v';
                        case '\\': return '\\';
                        default:
                            if (esc >= '0' && esc <= '7') {
                                int oct = esc - '0';
                                int digits = 1;
                                while (idx < len && str[idx] >= '0' && str[idx] <= '7' && digits < 3) {
                                    oct = oct * 8 + (str[idx++] - '0');
                                    digits++;
                                }
                                return static_cast<uint8_t>(oct & 0xFF);
                            }
                            return static_cast<uint8_t>(esc);
                    }
                }
                return static_cast<uint8_t>(str[idx++]);
            };

            uint8_t c1 = parseChar(i);

            // Character Ranges: c1-c2
            if (i < len && str[i] == '-' && i + 1 < len) {
                i++;
                uint8_t c2 = parseChar(i);
                if (c1 <= c2) {
                    for (int c = c1; c <= c2; ++c) {
                        result.push_back(static_cast<uint8_t>(c));
                    }
                } else {
                    result.push_back(c1);
                    result.push_back(static_cast<uint8_t>('-'));
                    result.push_back(c2);
                }
            } else {
                result.push_back(c1);
            }
        }

        return result;
    }
