#include "purge.hpp"
#include "purge_app.hpp"

int main(int argc, char* argv[]) {
    PurgeApplication app;
    return app.Run(argc, argv);
}