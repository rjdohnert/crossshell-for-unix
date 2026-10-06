#include "recode_app.hpp"
#include "recode_engine.hpp"
#include "recode_options.hpp"

int runRecode(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    try {
        RecodeOptions options = RecodeOptions::parse(argc, argv);
        RecodeEngine engine(options);
        return engine.run();
    } catch (const std::exception& ex) {
        std::cerr << "recode error: " << ex.what() << "\n";
        return 1;
    }
}
