#pragma once

#include "gsar_types.hpp"

class GSARApp {
private:
    Options m_options;

    bool processFile(const std::filesystem::path& inputPath,
                     const std::filesystem::path* outputPath);
    int runFilterMode();

public:
    explicit GSARApp(Options options);
    int run();
};
