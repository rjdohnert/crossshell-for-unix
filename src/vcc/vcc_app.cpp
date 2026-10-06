#include "build_options.hpp"
#include "compiler_driver.hpp"
#include "flag_translator.hpp"
#include "vcc_app.hpp"

int runVcc(int argc, char* argv[]) {
    BuildOptions opts;
    FlagTranslator translator;

    if (!translator.Parse(argc, argv, opts)) {
        return 1;
    }

    CompilerDriver driver;
    return driver.Run(opts);
}
