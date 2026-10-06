#include "paste_app.hpp"

int PasteApplication::Run(int argc, char* argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    PasteOptions opts;
    if (!m_parser.Parse(argc, argv, opts)) {
        OptionParser::PrintUsage(argc > 0 ? argv[0] : "paste");
        return 1;
    }

    if (opts.show_help) {
        OptionParser::PrintUsage(argc > 0 ? argv[0] : "paste");
        return 0;
    }

    if (opts.show_version) {
        OptionParser::PrintVersion();
        return 0;
    }

    std::vector<std::string> delims = DelimiterParser::Parse(opts.delim_list);
    PasteEngine engine(std::move(delims));

    if (opts.serial) {
        engine.ExecuteSerial(opts.files);
    } else {
        engine.ExecuteParallel(opts.files);
    }

    return 0;
}
