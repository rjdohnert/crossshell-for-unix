#pragma once

#include "strings_options.hpp"
#include "strings.hpp"

class StringsEngine {
private:
    StringsOptions options;

    static bool isPrintable(unsigned char c);

    void processStream(std::istream& in, std::vector<std::string>& results) const;

public:
    explicit StringsEngine(StringsOptions opts);

    int execute();
};
