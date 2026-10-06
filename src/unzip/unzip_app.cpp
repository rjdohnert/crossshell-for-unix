#include "unzip_app.hpp"
#include "unzip_engine.hpp"
#include "unzip_option_parser.hpp"
#include "unzip_options.hpp"

int runUnzip(int argc, char* argv[]) {
    UnzipOptions options;
    auto result = parseUnzipOptions(argc, argv, options);
    if (result) return *result;
    return executeUnzip(options);
}
