#include "lsbt_app.hpp"

int main(int argc, char* argv[]) {
    auto opts = CommandLineParser::parse(argc, argv);
    LsbtApp app(opts);
    return app.run();
}