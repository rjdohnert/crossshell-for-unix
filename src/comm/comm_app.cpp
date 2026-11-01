#include "comm_app.h"
#include "options.h"
#include "exit_code.h"
#include "arg_parser.h"
#include "help_system.h"
#include "comm_engine.h"

namespace CommUtil {

int CommApp::run(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Options opts;
    if (!ArgParser::parse(argc, argv, opts)) {
        return static_cast<int>(ExitCode::Failure);
    }

    if (opts.show_help) {
        HelpSystem::print_help();
        return static_cast<int>(ExitCode::Success);
    }

    if (opts.show_version) {
        HelpSystem::print_version();
        return static_cast<int>(ExitCode::Success);
    }

    CommEngine engine(opts);
    return static_cast<int>(engine.execute());
}

} // namespace CommUtil
