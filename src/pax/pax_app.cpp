#include "pax_app.hpp"

int PaxApplication::Run(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    PaxOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        return 1;
    }
    if (exitEarly) {
        return 0;
    }

    // MODE 4: COPY MODE (-r -w)
    if (opts.mode_read && opts.mode_write) {
        if (opts.positionals.size() < 2) {
            std::cerr << "pax: copy mode requires source files and a target directory\n";
            return 1;
        }
        fs::path dest_dir = opts.positionals.back();
        std::vector<fs::path> sources(opts.positionals.begin(), opts.positionals.end() - 1);
        PaxArchiver::Copy(sources, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
        return 0;
    }

    // MODE 2: READ / EXTRACT MODE (-r)
    if (opts.mode_read && !opts.mode_write) {
        fs::path dest_dir = opts.positionals.empty() ? "." : opts.positionals[0];
        if (!opts.archive_file.empty()) {
            std::ifstream in(opts.archive_file, std::ios::binary);
            if (!in) {
                std::cerr << "pax: cannot open archive " << opts.archive_file << "\n";
                return 1;
            }
            PaxArchiver::Read(in, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
        } else {
            PaxArchiver::Read(std::cin, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
        }
        return 0;
    }

    // MODE 3: WRITE / ARCHIVE MODE (-w)
    if (!opts.mode_read && opts.mode_write) {
        if (opts.positionals.empty()) {
            std::cerr << "pax: write mode requires file/directory arguments\n";
            return 1;
        }
        if (!opts.archive_file.empty()) {
            std::ofstream out(opts.archive_file, std::ios::binary);
            if (!out) {
                std::cerr << "pax: cannot create archive " << opts.archive_file << "\n";
                return 1;
            }
            PaxArchiver::Write(out, opts.positionals, opts.verbose, opts.substitutions);
        } else {
            PaxArchiver::Write(std::cout, opts.positionals, opts.verbose, opts.substitutions);
        }
        return 0;
    }

    // MODE 1: LIST MODE (default)
    if (!opts.archive_file.empty()) {
        std::ifstream in(opts.archive_file, std::ios::binary);
        if (!in) {
            std::cerr << "pax: cannot open archive " << opts.archive_file << "\n";
            return 1;
        }
        PaxArchiver::List(in, opts.verbose, opts.substitutions);
    } else {
        PaxArchiver::List(std::cin, opts.verbose, opts.substitutions);
    }

    return 0;
}
