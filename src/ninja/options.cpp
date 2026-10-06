#include "options.hpp"

namespace EnterpriseNinja {

void NinjaOptionsParser::PrintHelp() {
    std::cout <<
R"(usage: ninja [options] [targets...]

Ninja Build System v3.0.0

options:
  --version      print ninja version ("3.0.0")
  -v, --verbose  show all command lines while building
  -C DIR         change to DIR before doing anything else
  -f FILE        specify input build file [default=build.ninja]
  -j N           run N jobs in parallel [default=CPU count]
  -k N           keep going until N jobs fail (0 means keep going forever) [default=1]
  -l N           do not start new jobs if the load average is greater than N
  -n             dry run (don't run commands but pretend they succeeded)
  -d MODE        enable debugging modes (stats, explain, keepdepfile)
  -t TOOL        run a subtool (clean, targets, graph)
  -h, --help     print this message
)";
}

void NinjaOptionsParser::PrintVersion() {
    std::cout << "3.0.0\n";
}

NinjaOptions NinjaOptionsParser::Parse(int argc, char** argv) {
    NinjaOptions opts;
    int detected_jobs = static_cast<int>(std::thread::hardware_concurrency());
    opts.jobs = (detected_jobs > 0) ? detected_jobs : 4;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return opts;
        } else if (arg == "--version") {
            opts.show_version = true;
            return opts;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-n") {
            opts.dry_run = true;
        } else if (arg == "-f" && i + 1 < argc) {
            opts.manifest = argv[++i];
        } else if (arg == "-j" && i + 1 < argc) {
            opts.jobs = std::stoi(argv[++i]);
        } else if (arg == "-k" && i + 1 < argc) {
            opts.keep_going = std::stoi(argv[++i]);
        } else if (arg == "-C" && i + 1 < argc) {
            std::error_code ec;
            std::filesystem::current_path(argv[++i], ec);
            if (ec) {
                opts.valid = false;
                opts.error_message = "cannot change directory to '" + std::string(argv[i]) + "'";
                return opts;
            }
        } else if (arg[0] == '-') {
            std::cerr << "ninja: warning: flag '" << arg << "' ignored in enterprise baseline mode\n";
        } else {
            opts.target_names.push_back(arg);
        }
    }

    return opts;
}

} // namespace EnterpriseNinja
