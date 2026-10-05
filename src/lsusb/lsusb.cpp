#include "lsusb_app.hpp"

int main(int argc, char* argv[]) {
    auto opts = CommandLineParser::parse(argc, argv);
    LsusbApp app(opts);
    return app.run();
}
