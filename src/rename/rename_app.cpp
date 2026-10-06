#include "command_line_parser.hpp"
#include "file_renamer.hpp"
#include "help_formatter.hpp"
#include "rename_app.hpp"
#include "rename_options.hpp"

int runRename(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);

    RenameOptions options;
    if (!CommandLineParser::parse(argc, argv, options)) {
        HelpFormatter::printUsage(std::cerr);
        return 2;
    }

    if (options.showHelp) {
        HelpFormatter::printHelp(std::cout);
        return 0;
    }

    if (options.showVersion) {
        HelpFormatter::printVersion(std::cout);
        return 0;
    }

    FileRenamer engine(std::move(options));
    return engine.execute();
}
