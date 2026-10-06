#include "make_app.hpp"
#include "engine.hpp"
#include <iostream>
#include <chrono>
#include <windows.h>

MakeApp::MakeApp(Config config) : cfg(std::move(config)) {}

int MakeApp::run() {
    if (cfg.show_help) {
        CommandLineParser::print_help();
        return 0;
    }

    if (cfg.show_version) {
        CommandLineParser::print_version();
        return 0;
    }

    if (!Color::enabled) {
        Color::RESET = Color::RED = Color::GREEN = Color::YELLOW = Color::BLUE = Color::CYAN = Color::BOLD = "";
    }

    if (!cfg.change_dir.empty()) {
        std::error_code ec;
        fs::current_path(cfg.change_dir, ec);
        if (ec) {
            std::cerr << Color::RED << "make: *** Unable to change directory to " << cfg.change_dir.string() << Color::RESET << "\n";
            return 1;
        }
    }

    if (cfg.makefile_path.empty()) {
        if (fs::exists("Makefile")) cfg.makefile_path = "Makefile";
        else if (fs::exists("makefile")) cfg.makefile_path = "makefile";
        else if (fs::exists("Makefile.win")) cfg.makefile_path = "Makefile.win";
        else {
            std::cerr << Color::RED << "make: *** No targets specified and no makefile found. Stop." << Color::RESET << "\n";
            return 1;
        }
    }

    // Apply CLI overrides during parser creation
    MakefileParser parser(cfg.cli_macros);
    if (!parser.parse(cfg.makefile_path)) {
        std::cerr << Color::RED << "make: *** Failed to parse makefile '" << cfg.makefile_path << "'. Stop." << Color::RESET << "\n";
        return 1;
    }

    ParallelBuildEngine engine(cfg, parser);

    if (cfg.export_compile_commands) {
        engine.export_compilation_database();
        std::cout << Color::GREEN << "make: Exported compile_commands.json\n" << Color::RESET;
    }

    std::vector<std::string> goals = cfg.target_goals;
    if (goals.empty()) {
        if (!parser.default_target.empty()) {
            goals.push_back(parser.default_target);
        } else {
            std::cerr << Color::RED << "make: *** No target found. Stop." << Color::RESET << "\n";
            return 1;
        }
    }

    auto build_start = std::chrono::high_resolution_clock::now();
    bool success = engine.execute(goals);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - build_start
    ).count();

    if (cfg.json_output) {
        engine.dump_json_telemetry();
    } else if (success && !cfg.silent) {
        std::cout << Color::GREEN << "make: Target(s) built successfully in " 
                  << elapsed << " ms." << Color::RESET << "\n";
    }

    return success ? 0 : 1;
}
