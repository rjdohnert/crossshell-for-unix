#include "http_downloader.hpp"
#include "http_url.hpp"
#include "url_filename.hpp"
#include "wget_app.hpp"
#include "wget_help.hpp"

int runWget(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" || std::string(argv[1]) == "/?" || std::string(argv[1]) == "-?") {
        PrintUsage(argc > 0 ? argv[0] : "wget");
        return argc < 2 ? 1 : 0;
    }

    OutputFormat outputFormat = OutputFormat::Human;
    std::string pipeCommand;
    std::string url;
    std::string filename;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") { PrintUsage(argv[0]); return 0; }
        if (arg == "-v" || arg == "--version") { std::cout << "wget 2.0\n"; return 0; }
        if (arg == "--json") outputFormat = OutputFormat::Json;
        else if (arg == "--csv") outputFormat = OutputFormat::Csv;
        else if (arg == "--table") outputFormat = OutputFormat::Table;
        else if (arg == "--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else if (url.empty()) url = arg;
        else if (filename.empty()) filename = arg;
        else { std::cerr << "Error: unexpected argument " << arg << "\n"; return 1; }
    }
    if (!IsHttpUrl(url)) {
        std::cerr << "Error: Only HTTP/HTTPS URLs are supported.\n";
        return 1;
    }

    if (filename.empty()) filename = GetFilenameFromUrl(url);
    return download_http_file(url, filename, outputFormat, pipeCommand);
}
