#include "printf_app.hpp"

int PrintfApplication::Run(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    PrintfOptions opts;
    if (!m_parser.Parse(argc, argv, opts)) {
        if (opts.show_help) {
            OptionParser::PrintHelp();
            return 0;
        }
        if (opts.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }
        std::cerr << "usage: printf [-v var] format [argument ...]\n";
        return 1;
    }

    if (opts.show_help) {
        OptionParser::PrintHelp();
        return 0;
    }

    if (opts.show_version) {
        OptionParser::PrintVersion();
        return 0;
    }

    if (!opts.varName.empty()) {
        std::ostringstream ss;
        FormatEngine::ExecuteFormatLoop(opts.format, opts.args, ss);
        SetEnvironmentVariableA(opts.varName.c_str(), ss.str().c_str());
    } else {
        FormatEngine::ExecuteFormatLoop(opts.format, opts.args, std::cout);
    }
    return 0;
}
