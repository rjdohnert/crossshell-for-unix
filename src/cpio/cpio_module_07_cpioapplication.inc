class CpioApplication {
public:
    int Run(int argc, char* argv[]) const {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        CpioOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(argv[0]);
            return 0;
        }
        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.mode == CpioMode::None) {
            opts.PrintHelp(argv[0]);
            return 1;
        }

        if (opts.mode == CpioMode::Create) {
            if (!opts.archive_file.empty()) {
                std::ofstream fout(opts.archive_file, std::ios::binary);
                if (!fout) {
                    std::cerr << "cpio: cannot open " << opts.archive_file << "\n";
                    return 1;
                }
                CpioArchiveEngine::DoCreate(fout, opts.null_delim, opts.verbose);
            } else {
                CpioArchiveEngine::DoCreate(std::cout, opts.null_delim, opts.verbose);
            }
        } else if (opts.mode == CpioMode::Extract || opts.mode == CpioMode::List) {
            bool extract = (opts.mode == CpioMode::Extract);
            if (!opts.archive_file.empty()) {
                std::ifstream fin(opts.archive_file, std::ios::binary);
                if (!fin) {
                    std::cerr << "cpio: cannot open " << opts.archive_file << "\n";
                    return 1;
                }
                CpioArchiveEngine::DoExtractOrList(fin, extract, opts.make_dirs, opts.verbose);
            } else {
                CpioArchiveEngine::DoExtractOrList(std::cin, extract, opts.make_dirs, opts.verbose);
            }
        } else if (opts.mode == CpioMode::Pass) {
            if (opts.pass_dir.empty()) {
                std::cerr << "cpio: destination directory required for pass-through mode (-p)\n";
                return 1;
            }
            CpioArchiveEngine::DoPass(opts.pass_dir, opts.null_delim, opts.make_dirs, opts.verbose);
        }

        if (opts.outputFormat != OutputFormat::Default || !opts.pipeCommand.empty()) {
            std::string modeName = (opts.mode == CpioMode::Create) ? "create" :
                                   (opts.mode == CpioMode::Extract) ? "extract" :
                                   (opts.mode == CpioMode::List) ? "list" : "pass-through";
            std::string text = (opts.outputFormat == OutputFormat::Json) ? "{\"status\":\"success\",\"mode\":\"" + modeName + "\"}\n" :
                               (opts.outputFormat == OutputFormat::Csv) ? "status,mode\nsuccess," + modeName + "\n" :
                               "STATUS\tMODE\nsuccess\t" + modeName + "\n";

            if (!opts.pipeCommand.empty()) {
                FILE* pipe = _popen(opts.pipeCommand.c_str(), "w");
                if (pipe) {
                    fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else {
                std::cout << text;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    CpioApplication app;
    return app.Run(argc, argv);
}
