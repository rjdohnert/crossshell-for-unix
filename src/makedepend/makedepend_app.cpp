#include "makedepend_app.hpp"

int MakedependApp::run(const MakedependOptions& opts) {
    if (opts.showHelp) {
        print_help();
        return 0;
    }
    if (opts.showVersion) {
        print_version();
        return 0;
    }

    DependencyParser parser;
    parser.verbose = opts.verbose;

    if (!opts.gccVer.empty()) {
        parser.setup_gcc_defaults(opts.gccVer);
    } else if (!opts.clangVer.empty()) {
        parser.setup_clang_defaults(opts.clangVer);
    } else {
        parser.setup_msvc_defaults(opts.msvcVer);
    }

    for (const auto& dir : opts.includeDirs) {
        parser.includeDirs.push_back(dir);
    }

    for (const auto& def : opts.defines) {
        parser.add_object_macro(def.first, def.second);
    }

    for (const auto& sym : opts.undefines) {
        parser.macros.erase(sym);
    }

    if (opts.autoDetectCL)    parser.load_env_includes();
    if (opts.autoDetectGCC)   parser.load_gcc_includes();
    if (opts.autoDetectClang) parser.load_clang_includes();

    if (opts.sourceFiles.empty()) {
        std::cerr << "makedepend: No source files specified.\n";
        return 1;
    }

    std::map<std::string, std::set<std::string, CaseInsensitivePathCompare>> allDependencies;

    for (const auto& src : opts.sourceFiles) {
        if (!fs::exists(src)) {
            std::cerr << "makedepend: Skipping non-existent file: " << src << "\n";
            continue;
        }

        fs::path p(src);
        std::string stem = p.stem().string();
        std::string target = opts.objPrefix + stem + opts.objSuffix;

        if (parser.verbose) {
            std::cout << "makedepend: Parsing " << src << " -> " << target << "\n";
        }

        allDependencies[target] = parser.find_dependencies(src);
    }

    update_makefile(opts.makefilePath, opts.delimiter, opts.appendOnly, allDependencies);

    if (parser.verbose) {
        std::cout << "makedepend: Successfully updated " << opts.makefilePath << "\n";
    }

    return 0;
}
