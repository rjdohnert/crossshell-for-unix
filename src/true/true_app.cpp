#include "true_app.hpp"
#include "true_options.hpp"
#include "true_reporter.hpp"

int TrueApp::run(int argc, wchar_t* argv[]) {
        TrueOptions options;
        if (!TrueOptions::parse(argc, argv, options)) {
            return 1;
        }
        return TrueReporter::report(options);
    }
