#include "mtr.hpp"
#include "options.hpp"
#include "mtr_app.hpp"

int main(int argc, char* argv[]) {
    try {
        WinsockGuard winsock;

        auto opts = CliParser::parse(argc, argv);
        if (!opts) {
            CliParser::displayHelp(argv[0]);
            return 1;
        }

        if (opts->showHelp) {
            CliParser::displayHelp(argv[0]);
            return 0;
        }

        if (opts->showVersion) {
            CliParser::displayVersion();
            return 0;
        }

        MTR app(*opts);
        if (!app.setup()) {
            return 1;
        }

        app.execute();
    }
    catch (const std::exception& ex) {
        std::cerr << "MTR Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}