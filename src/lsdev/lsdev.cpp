#include "lsdev_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        ArgumentParser parser;
        CommandLineOptions opts = parser.Parse(argc, argv);
        SetupApiDeviceEnumerator enumerator;
        DeviceFilter filter(opts);
        ConsoleDeviceRenderer renderer(opts);
        LsDevApp app(opts, enumerator, filter, renderer);
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "lsdev: fatal error: " << ex.what() << "\n";
        return 1;
    }
}