#include "rsync.hpp"
#include "rsync_options.hpp"
#include "sync_engine.hpp"

class RsyncApp {
public:
    static int run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        try {
            RsyncOptions options = RsyncOptions::parse(argc, argv);
            RsyncEngine engine(options);
            return engine.execute();
        } catch (const std::exception& ex) {
            std::cerr << "rsync error: " << ex.what() << "\n";
            return 1;
        }
    }
};

int main(int argc, char* argv[]) {
    return RsyncApp::run(argc, argv);
}