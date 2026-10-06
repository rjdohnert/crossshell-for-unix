#include "unzip_help.hpp"
#include "unzip_option_parser.hpp"
#include "unzip_options.hpp"

std::optional<int> parseUnzipOptions(int argc, char* argv[], UnzipOptions& opts) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    bool parsing_flags = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--json" || arg == "--csv" || arg == "--table") { opts.output_format = arg == "--json" ? OutputFormat::Json : (arg == "--csv" ? OutputFormat::Csv : OutputFormat::Table); continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-d") {
            if (i + 1 < argc) {
                opts.dest_dir = argv[++i];
            } else {
                std::cerr << "unzip error: -d option requires a directory argument\n";
                return 1;
            }
        } else if (arg.rfind("-d", 0) == 0 && arg.length() > 2) {
            opts.dest_dir = arg.substr(2);
        } else if (parsing_flags && arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (c == 'l') opts.list = true;
                else if (c == 't') opts.test = true;
                else if (c == 'q') opts.quiet = true;
                else if (c == 'o') opts.overwrite = true;
                else if (c == 'f') {
                    opts.overwrite = true;
                }
                else if (c == 'n') opts.never_overwrite = true;
                else if (c == 'j') opts.junk_paths = true;
                else if (c == '-') { parsing_flags = false; break; }
                else {
                    std::cerr << "unzip error: Unknown option -" << c << "\n";
                    return 1;
                }
            }
        } else {
            if (opts.zip_filename.empty()) {
                opts.zip_filename = arg;
                if (!fs::exists(opts.zip_filename)) {
                    fs::path p(opts.zip_filename);
                    std::string ext = p.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                    });
                    if (ext != ".zip" && fs::exists(opts.zip_filename + ".zip")) {
                        opts.zip_filename += ".zip";
                    }
                }
            } else {
                opts.filters.push_back(arg);
            }
        }
    }

    if (opts.zip_filename.empty()) {
        std::cerr << "unzip error: No zipfile specified.\n";
        return 1;
    }

    return std::nullopt;
}
