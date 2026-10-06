#include "ninja_app.hpp"

namespace EnterpriseNinja {

int NinjaApp::Run(int argc, char** argv) {
    ProcessTracker::InstallSignalHandlers();

    NinjaOptions options = NinjaOptionsParser::Parse(argc, argv);
    if (options.show_help) {
        NinjaOptionsParser::PrintHelp();
        ProcessTracker::Cleanup();
        return 0;
    }
    if (options.show_version) {
        NinjaOptionsParser::PrintVersion();
        ProcessTracker::Cleanup();
        return 0;
    }
    if (!options.valid) {
        std::cerr << "ninja: fatal: " << options.error_message << "\n";
        ProcessTracker::Cleanup();
        return 1;
    }

    BuildEngine engine;
    if (!ManifestParser::ParseFile(options.manifest, engine)) {
        ProcessTracker::Cleanup();
        return 1;
    }

    std::vector<Node*> targets;
    if (options.target_names.empty()) {
        if (!engine.default_targets.empty()) {
            targets = engine.default_targets;
        } else if (!engine.all_edges.empty()) {
            targets = engine.all_edges.front()->outputs;
        }
    } else {
        for (const auto& t : options.target_names) {
            targets.push_back(engine.GetOrCreateNode(t));
        }
    }

    bool success = engine.ExecuteBuild(targets, options.jobs, options.dry_run, options.verbose, options.keep_going);
    ProcessTracker::Cleanup();
    return success ? 0 : 1;
}

} // namespace EnterpriseNinja
