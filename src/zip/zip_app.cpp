#include "archive_creator.hpp"
#include "option_parser.hpp"
#include "zip_app.hpp"
#include "zip_options.hpp"

int runZip(int argc, char* argv[]) {
    ZipOptions options;
    bool exitEarly = false;
    int status = parse_zip_options(argc, argv, options, exitEarly);
    if (status != 0 || exitEarly) return status;
    return create_zip_archive(options);
}
