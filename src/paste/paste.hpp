#ifndef PASTE_HPP
#define PASTE_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

struct InputStream {
    bool is_stdin = false;
    std::ifstream file;
    bool eof = false;
};

class DelimiterParser {
public:
    static std::vector<std::string> Parse(const std::string& list);
};

class StreamReader {
public:
    static bool GetNextLine(InputStream& stream, std::string& line);
};

#endif // PASTE_HPP
