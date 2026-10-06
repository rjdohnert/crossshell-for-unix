#include "patch_app.hpp"

int PatchApplication::Run(int argc, char* argv[]) {
    PatchOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        return 1;
    }
    if (exitEarly) {
        return 0;
    }

    if (!opts.changeDir.empty()) {
        fs::current_path(opts.changeDir);
    }

    if (opts.positionalArgs.size() >= 1 && opts.outputFile.empty()) {
        opts.outputFile = opts.positionalArgs[0];
    }
    if (opts.positionalArgs.size() >= 2 && opts.patchFile.empty()) {
        opts.patchFile = opts.positionalArgs[1];
    }

    std::istream* patchStream = &std::cin;
    std::ifstream fileIn;

    if (!opts.patchFile.empty()) {
        fileIn.open(opts.patchFile);
        if (!fileIn.is_open()) {
            std::cerr << "patch: cannot open patch file '" << opts.patchFile << "'\n";
            return 1;
        }
        patchStream = &fileIn;
    }

    std::vector<FilePatch> patches = PatchParser::ParseUnifiedDiff(*patchStream);
    if (patches.empty()) {
        std::cerr << "patch: no valid unified diff hunks found.\n";
        return 1;
    }

    bool success = true;
    for (auto& patch : patches) {
        if (opts.reverse) {
            PatchEngine::ReverseFilePatch(patch);
        }
        if (!PatchEngine::ApplyPatchToFile(patch, opts)) {
            success = false;
        }
    }

    return success ? 0 : 1;
}
