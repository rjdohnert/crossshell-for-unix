// ============================================================================
// Command Line Interface Parser
// ============================================================================

class CliParser {
public:
    static bool parse(int argc, char* argv[], RobocopyOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") { opts.show_help = true; return true; }
            if (arg == "-V" || arg == "--version") { opts.show_version = true; return true; }

            // Copy & Tree Switches
            if (arg == "-s" || arg == "--subdirs") opts.subdirs = true;
            else if (arg == "-e" || arg == "--all-subdirs") opts.all_subdirs = true;
            else if (arg == "-m" || arg == "--mirror") opts.mirror = true;
            else if (arg == "--purge") opts.purge = true;
            else if (arg == "--move-files") opts.move_files = true;
            else if (arg == "--move-all") opts.move_all = true;
            else if (arg == "--create-zero") opts.create_zero = true;

            // Selection Switches
            else if (arg == "--exclude-changed") opts.exclude_changed = true;
            else if (arg == "--exclude-newer") opts.exclude_newer = true;
            else if (arg == "--exclude-older") opts.exclude_older = true;

            // Logging Switches
            else if (arg == "-n" || arg == "-L" || arg == "--dry-run") opts.dry_run = true;
            else if (arg == "-v" || arg == "--verbose") opts.verbose = true;
            else if (arg == "--no-progress") opts.no_progress = true;
            else if (arg == "--no-file-list") opts.no_file_list = true;
            else if (arg == "--no-dir-list") opts.no_dir_list = true;
            else if (arg == "--no-header") opts.no_header = true;
            else if (arg == "--no-summary") opts.no_summary = true;

            // Parameter Switches with Arguments
            else if (arg == "-l" || arg.rfind("--level=", 0) == 0) {
                opts.max_level = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-r" || arg.rfind("--retries=", 0) == 0) {
                opts.retries = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-w" || arg.rfind("--wait=", 0) == 0) {
                opts.wait_seconds = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-j" || arg == "-t" || arg.rfind("--threads=", 0) == 0) {
                opts.threads = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--exclude-files=", 0) == 0) {
                opts.exclude_files.push_back(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--exclude-dirs=", 0) == 0) {
                opts.exclude_dirs.push_back(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--max-size=", 0) == 0) {
                opts.max_size = std::stoull(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--min-size=", 0) == 0) {
                opts.min_size = std::stoull(extract_val(arg, argv, i, argc));
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "install: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'install --help' for more information.\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        if (positional.size() < 2) {
            std::cerr << "install: missing source and destination directories\n";
            std::cerr << "usage: install [options] <source> <destination> [file [file ...]]\n";
            return false;
        }

        opts.source = positional[0];
        opts.destination = positional[1];
        if (positional.size() > 2) {
            opts.file_specs.clear();
            for (size_t k = 2; k < positional.size(); ++k) {
                opts.file_specs.push_back(positional[k]);
            }
        }
        return true;
    }

private:
    static std::string extract_val(const std::string& arg, char* argv[], int& i, int argc) {
        size_t eq = arg.find('=');
        if (eq != std::string::npos) return arg.substr(eq + 1);
        if (i + 1 < argc) return argv[++i];
        return "";
    }
};

