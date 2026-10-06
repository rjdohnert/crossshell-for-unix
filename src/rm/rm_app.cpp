#include "option_parser.hpp"
#include "rm_app.hpp"
#include "rm_engine.hpp"
#include "rm_options.hpp"

int RmApplication::Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        RmOptions options;
        if (!m_parser.Parse(argc, argv, options)) {
            return 1;
        }

        if (options.showHelp) {
            OptionParser::ShowHelp();
            return 0;
        }

        if (options.showVersion) {
            OptionParser::ShowVersion();
            return 0;
        }

        RmEngine engine(options);
        return engine.Run();
    }
