#include "uniq_app.hpp"
#include "uniq_engine.hpp"
#include "uniq_options.hpp"

int UniqApp::run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        try {
            UniqOptions options = UniqOptions::parse(argc, argv);
            UniqEngine engine(std::move(options));
            return engine.execute();
        } catch (const std::exception& ex) {
            std::cerr << "uniq: fatal error: " << ex.what() << "\n";
            return 1;
        }
    }
