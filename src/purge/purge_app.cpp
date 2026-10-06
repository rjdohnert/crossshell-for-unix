#include "purge_app.hpp"

int PurgeApplication::Run(int argc, char* argv[]) const {
    PurgeOptions opts;
    opts.Parse(argc, argv);

    std::ofstream fileStream;
    std::ostream* outStream = &std::cout;

    if (!opts.outputSpec.empty()) {
        fileStream.open(opts.outputSpec, std::ios::out);
        if (!fileStream.is_open()) {
            std::cerr << "%RMS-F-CRE, error creating output file " << StringHelper::WStringToString(opts.outputSpec) << "\n";
            return 2;
        }
        outStream = &fileStream;
    }

    if (opts.help) {
        PurgeReporter::DisplayHelp(opts.helpTopic, *outStream);
        return 1;
    }

    return PurgeEngine::Execute(opts, *outStream);
}
