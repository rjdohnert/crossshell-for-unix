#ifndef ENTAB_MODELS_HPP
#define ENTAB_MODELS_HPP

#include <cstddef>
#include <string>
#include <vector>

enum class EntabOutputFormat {
    Human,
    Json,
    Csv,
    Table
};

struct EntabInput {
    std::string path;
    bool standardInput = false;
};

struct EntabOptions {
    static constexpr std::size_t IO_BUFFER_SIZE = 64 * 1024;

    std::vector<std::size_t> tabStops;
    std::vector<EntabInput> files;
    std::size_t tabWidth = 8;
    EntabOutputFormat outputFormat = EntabOutputFormat::Human;
    std::string pipeCommand;
    bool convertAll = false;
    bool firstOnly = false;
    bool showHelp = false;
    bool showVersion = false;
};

struct EntabParseResult {
    bool success = false;
    std::string error;
    EntabOptions options;
};

#endif
