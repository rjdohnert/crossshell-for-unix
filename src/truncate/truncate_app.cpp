#include "file_truncator.hpp"
#include "size_calculator.hpp"
#include "truncate_app.hpp"
#include "truncate_help.hpp"

int TruncateApp::parse(int argc, char* argv[]) {
        if (argc < 2) {
            std::cerr << "usage: truncate [-c] -s [+|-|%|/]size[b|k|m|g|t|p|e] file ...\n";
            std::cerr << "       truncate [-c] -r rfile file ...\n";
            return 1;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return 0;
            }
            if (arg == "-V" || arg == "--version") {
                options.show_version = true;
                return 0;
            }

            if (arg == "-c") {
                options.no_create = true;
            } else if (arg == "-s") {
                if (i + 1 >= argc) {
                    std::cerr << "truncate: option requires an argument -- s\n";
                    return 1;
                }
                std::string err;
                if (!SizeCalculator::parse(argv[++i], options.size_calc, err)) {
                    std::cerr << "truncate: " << err << "\n";
                    return 1;
                }
                options.has_size = true;
            } else if (arg == "-r") {
                if (i + 1 >= argc) {
                    std::cerr << "truncate: option requires an argument -- r\n";
                    return 1;
                }
                std::string r_path = argv[++i];
                int wlen = MultiByteToWideChar(CP_UTF8, 0, r_path.c_str(), -1, NULL, 0);
                std::wstring wstr(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, r_path.c_str(), -1, &wstr[0], wlen);
                wstr.pop_back(); // Remove null terminator from std::wstring length
                options.ref_file = wstr;
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "truncate: unknown option -- " << arg << "\n";
                std::cerr << "Try 'truncate --help' for more information.\n";
                return 1;
            } else {
                options.files.push_back(arg);
            }
        }

        if (!options.has_size && options.ref_file.empty()) {
            std::cerr << "truncate: must specify either -s or -r\n";
            return 1;
        }
        if (options.has_size && !options.ref_file.empty()) {
            std::cerr << "truncate: -s and -r cannot be specified together\n";
            return 1;
        }
        if (options.files.empty()) {
            std::cerr << "truncate: no files specified\n";
            return 1;
        }

        return 0;
    }

int TruncateApp::run() {
        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }
        if (options.show_version) {
            std::cout << "truncate 2.0\n";
            return 0;
        }

        FileTruncator truncator;
        truncator.set_no_create(options.no_create);

        if (options.has_size) {
            truncator.set_size_calculator(options.size_calc);
        } else {
            truncator.set_reference_file(options.ref_file);
        }

        int failed_count = 0;
        for (const auto& file_str : options.files) {
            std::string err_msg;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, file_str.c_str(), -1, NULL, 0);
            if (wlen == 0) {
                std::cerr << "truncate: " << file_str << ": invalid UTF-8 path\n";
                failed_count++;
                continue;
            }
            std::wstring wpath(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, file_str.c_str(), -1, &wpath[0], wlen);
            wpath.pop_back();
            if (!truncator.truncate_file(wpath, err_msg)) {
                std::cerr << "truncate: " << file_str << ": " << err_msg << "\n";
                failed_count++;
            }
        }

        return (failed_count == 0) ? 0 : 1;
    }
