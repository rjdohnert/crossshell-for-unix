#include "archive_extension.hpp"
#include "option_parser.hpp"
#include "zip_help.hpp"
#include "zip_options.hpp"

int parse_zip_options(int argc, char* argv[], ZipOptions& opts, bool& exitEarly) {
    exitEarly = false;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    bool parsing_flags = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--json" || arg == "--csv" || arg == "--table") { opts.output_format = arg == "--json" ? OutputFormat::Json : (arg == "--csv" ? OutputFormat::Csv : OutputFormat::Table); continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }

        if (parsing_flags && arg == "--help") {
            print_usage(argv[0]);
            exitEarly = true;
            return 0;
        }

        if (parsing_flags && arg[0] == '-' && arg.length() > 1) {
            if (arg == "-f") {
                opts.force = true;
            } else if (arg == "-x" && i + 1 < argc) {
                opts.exclude_paths.push_back(argv[++i]);
            } else {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (c == 'r') opts.recursive = true;
                    else if (c == 'j') opts.junk_paths = true;
                    else if (c == 'q') opts.quiet = true;
                    else if (c == 'f') opts.force = true;
                    else if (c >= '0' && c <= '9') opts.compression_level = c - '0';
                    else if (c == '-') { parsing_flags = false; break; }
                    else {
                        std::cerr << "zip error: Invalid option -" << c << "\n";
                        return 1;
                    }
                }
            }
        } else {
            if (opts.zip_filename.empty()) {
                opts.zip_filename = arg;
                // Auto-append .zip extension if missing
                if (!is_zip_ext(fs::path(opts.zip_filename))) {
                    opts.zip_filename += ".zip";
                }
            } else {
                opts.input_paths.push_back(arg);
            }
        }
    }

    if (opts.zip_filename.empty() || opts.input_paths.empty()) {
        std::cerr << "zip error: Nothing to do! (must specify zipfile and input files)\n";
        return 1;
    }

    return 0;
}
