#include "search.hpp"
#include "terminal_format.hpp"
#include "search_options.hpp"
#include "search_reporter.hpp"
#include "search_engine.hpp"

class SearchApplication {
private:
    OptionParser m_parser;
    SearchEngine m_engine;

public:
    int Run(int argc, char* argv[]) {
        ConsoleTerminal::EnableVirtualTerminal();

        if (argc == 1) {
            SearchReporter::PrintBanner();
        }

        SearchOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            return (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" || std::string(argv[1]) == "/?")) ? 0 : 1;
        }

        return m_engine.Execute(opt);
    }
};

int main(int argc, char* argv[]) {
    SearchApplication app;
    return app.Run(argc, argv);
}