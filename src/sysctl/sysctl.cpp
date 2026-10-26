#include "sysctl_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        SysctlApp app;
        return app.Run(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "sysctl: fatal exception: " << ex.what() << "\n";
        return 1;
    }
}
