// ============================================================================
// 7. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================
class OptionParser {
public:
    static void DisplayHelp() {
        std::cout << R"(grep(1)             CrossShell for UNIX Reference Manual                 grep(1)

    NAME
        grep - search for patterns in files or input streams

    SYNOPSIS
        grep [OPTIONS] PATTERN [FILE...]
        grep [OPTIONS] -e PATTERN... [FILE...]

    DESCRIPTION
        grep searches for PATTERN in each FILE or standard input. It processes
        both plain text lines and structured objects (JSON Lines / NDJSON),
        permitting deep object navigation (.key, .user.id), numeric and predicate
        filtering (--where), and field extraction (--extract).

    OPTIONS
        -e, --regexp PATTERN
            Specify one or more matching patterns.

        -i, --ignore-case
            Perform case-insensitive matching.

        -E, --extended-regexp
            Interpret PATTERN as an extended regular expression.

        -F, --fixed-strings
            Interpret PATTERN as a fixed string, not a regular expression.

        -P, --perl-regexp
            Request Perl-compatible regular expressions.

        -v, --invert-match
            Invert the sense of matching to select non-matching records.

        -q, --quiet, --silent
            Suppress all normal output; return only exit match status.

        -s, --no-messages
            Suppress error messages about nonexistent or unreadable files.

        -a, --text
            Process binary input as text.

        -b, --byte-offset
            Prefix each output line with its 0-based byte offset in the input.

        -w, --word-regexp
            Force PATTERN to match only whole words.

        -x, --line-regexp
            Force PATTERN to match only exact entire lines.

        -k, --key PATH
            Search PATTERN exclusively inside target object key or array path.

        --where EXPR
            Filter records using expression predicate comparisons.

        --extract PATH
            Output only the target nested object value rather than the whole line.

        -j, --json-only
            Force pure JSON mode; non-JSON lines are ignored.

        -t, --text-only
            Disable object autodetection; treat input purely as plain text.

        -n, --line-number
            Prefix each output line with its 1-based line number.

        -c, --count
            Suppress normal output; print a count of matching lines instead.

        -l, --files-with-matches
            Suppress normal output; print name of each file with matches.

        -L, --files-without-match
            Suppress normal output; print name of each file with no matches.

        -o, --only-matching
            Print only the matched non-empty parts of matching lines.

        -H, --with-filename
            Print the file name for each match.

        -h, --no-filename
            Suppress the file name prefix on output.

        -m, --max-count NUM
            Stop reading a file after NUM matching lines.

        --color[=WHEN]
            Highlight matching strings; WHEN is 'always', 'never', or 'auto'.

        -B, --before-context NUM
            Print NUM lines of leading context before matching lines.

        -A, --after-context NUM
            Print NUM lines of trailing context after matching lines.

        -C, --context NUM
            Print NUM lines of leading and trailing context.

        -r, -R, --recursive
            Read all files under each directory recursively.

        -d, --directories ACTION
            Handle directories with ACTION: read, recurse, or skip.

        -D, --devices ACTION
            Handle devices with ACTION: read or skip.

        -I, --binary-files=without-match
            Treat binary files as having no matches.

        -Z, --null
            Terminate file names and matching output with a NUL byte.

        -U, --binary
            Read binary input without special binary-file handling.

        --registry KEY
            Search registry values emitted as JSON Lines.

        --wmi QUERY[|NAMESPACE]
            Search WMI objects emitted as JSON Lines.

        --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        grep "error" logfile.txt
            Search for literal or regex "error" in logfile.txt.

        grep -n "failed" app.log
            Display matching lines with line numbers.

        grep -E "warn|error" *.log
            Search using extended regular expression pattern.

        grep --where ".status >= 400 && .env == 'prod'" events.jsonl
            Filter JSON Lines using predicate expression.

    CrossShell for UNIX                                                    grep(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "grep version 2.0.0\n"
                  << "Copyright (c) 2026, Roberto J Dohnert\n";
    }

    bool Parse(int argc, char* argv[], GrepOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-?") { DisplayHelp(); exitEarly = true; return true; }
            if (arg == "--version" || arg == "-V") { DisplayVersion(); exitEarly = true; return true; }
            else if ((arg == "-e" || arg == "--regexp") && i + 1 < argc) opts.patterns.push_back(argv[++i]);
            else if ((arg == "-k" || arg == "--key") && i + 1 < argc) opts.key_path = argv[++i];
            else if (arg == "--where" && i + 1 < argc) opts.where_expr = argv[++i];
            else if (arg == "--extract" && i + 1 < argc) opts.extract_path = argv[++i];
            else if (arg == "--ignore-case") opts.ignore_case = true;
            else if (arg == "-E" || arg == "--extended-regexp") { }
            else if (arg == "-F" || arg == "--fixed-strings") opts.fixed_strings = true;
            else if (arg == "-P" || arg == "--perl-regexp") {
                std::cerr << "grep: Perl-compatible regular expressions are not supported\n";
                return false;
            }
            else if (arg == "--invert-match") opts.invert_match = true;
            else if (arg == "-q" || arg == "--quiet" || arg == "--silent") opts.quiet = true;
            else if (arg == "-s" || arg == "--no-messages") opts.no_messages = true;
            else if (arg == "-a" || arg == "--text") opts.text_mode = true;
            else if (arg == "-b" || arg == "--byte-offset") opts.byte_offset = true;
            else if (arg == "--word-regexp") opts.word_regexp = true;
            else if (arg == "--line-regexp") opts.line_regexp = true;
            else if (arg == "--line-number") opts.line_number = true;
            else if (arg == "--count") opts.count_only = true;
            else if (arg == "--files-with-matches") opts.files_with_matches = true;
            else if (arg == "--files-without-match") opts.files_without_match = true;
            else if (arg == "--only-matching") opts.only_matching = true;
            else if (arg == "--with-filename") opts.with_filename = true;
            else if (arg == "--no-filename") opts.no_filename = true;
            else if (arg == "--recursive") opts.recursive = true;
            else if ((arg == "-d" || arg == "--directories") && i + 1 < argc) opts.directory_action = argv[++i];
            else if ((arg == "-D" || arg == "--devices") && i + 1 < argc) opts.devices_action = argv[++i];
            else if (arg == "-I" || arg == "--binary-files=without-match") opts.binary_files_without_match = true;
            else if (arg == "-Z" || arg == "--null") opts.null_output = true;
            else if (arg == "-U" || arg == "--binary") opts.binary_mode = true;
            else if (arg == "-j" || arg == "--json-only") opts.json_mode = true;
            else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
            else if (arg == "-t" || arg == "--text-only") opts.text_only = true;
            else if (arg == "--color" || arg == "--color=always") { opts.color = true; opts.color_explicit = true; }
            else if (arg == "--color=never") { opts.color = false; opts.color_explicit = true; }
            else if ((arg == "-m" || arg == "--max-count") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.max_count = value;
            }
            else if ((arg == "-B" || arg == "--before-context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.before_context = value;
            }
            else if ((arg == "-A" || arg == "--after-context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.after_context = value;
            }
            else if ((arg == "-C" || arg == "--context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.before_context = value;
                opts.after_context = value;
            } else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                // Clustered short flags (e.g. -in, -rn, -v, -ePATTERN)
                bool stop_cluster = false;
                for (size_t j = 1; j < arg.size() && !stop_cluster; ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'i': opts.ignore_case = true; break;
                        case 'E': break;
                        case 'F': opts.fixed_strings = true; break;
                        case 'q': opts.quiet = true; break;
                        case 's': opts.no_messages = true; break;
                        case 'a': opts.text_mode = true; break;
                        case 'b': opts.byte_offset = true; break;
                        case 'v': opts.invert_match = true; break;
                        case 'w': opts.word_regexp = true; break;
                        case 'x': opts.line_regexp = true; break;
                        case 'n': opts.line_number = true; break;
                        case 'c': opts.count_only = true; break;
                        case 'l': opts.files_with_matches = true; break;
                        case 'L': opts.files_without_match = true; break;
                        case 'o': opts.only_matching = true; break;
                        case 'H': opts.with_filename = true; break;
                        case 'h': opts.no_filename = true; break;
                        case 'r': case 'R': opts.recursive = true; break;
                        case 'j': opts.json_mode = true; break;
                        case 't': opts.text_only = true; break;
                        case 'Z': opts.null_output = true; break;
                        case 'U': opts.binary_mode = true; break;
                        case 'e': {
                            std::string pat = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "");
                            if (!pat.empty()) opts.patterns.push_back(pat);
                            stop_cluster = true;
                            break;
                        }
                        case 'k': {
                            std::string key = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "");
                            if (!key.empty()) opts.key_path = key;
                            stop_cluster = true;
                            break;
                        }
                        case 'm': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -m\n";
                                return false;
                            }
                            opts.max_count = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'A': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -A\n";
                                return false;
                            }
                            opts.after_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'B': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -B\n";
                                return false;
                            }
                            opts.before_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'C': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -C\n";
                                return false;
                            }
                            opts.before_context = parsed;
                            opts.after_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        default:
                            std::cerr << "grep: unknown option -- '" << c << "'\n";
                            return false;
                    }
                }
            } else if (!arg.empty() && arg[0] != '-') {
                if (opts.patterns.empty() && opts.where_expr.empty()) {
                    opts.patterns.push_back(arg);
                } else {
                    opts.file_paths.push_back(arg);
                }
            }
        }

        if (!opts.color_explicit) {
#ifdef _WIN32
            opts.color = _isatty(_fileno(stdout)) != 0;
#else
            opts.color = isatty(fileno(stdout)) != 0;
#endif
        }

        return true;
    }
};

class GrepApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        GrepOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 2;
        }
        if (exitEarly) {
            return 0;
        }

        if (opts.patterns.empty() && opts.where_expr.empty()) {
            std::cerr << "grep: no pattern or predicate expression provided.\nTry 'grep --help' for more information.\n";
            return 2;
        }

        GrepEngine engine(opts);
        StreamProcessor processor(opts, engine);
        std::vector<std::string> final_files;
        bool has_io_error = false;
        if (opts.recursive && opts.file_paths.empty() && opts.object_sources.empty()) {
            opts.file_paths.push_back(".");
        }
        for (const auto& path_str : opts.file_paths) {
            if (fs::is_directory(path_str)) {
                if (opts.directory_action == "skip") continue;
                if (opts.directory_action == "recurse" || opts.recursive) {
                    processor.CollectFilesRecursive(path_str, final_files);
                } else {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << path_str << ": Is a directory\n";
                    has_io_error = true;
                }
            } else {
                final_files.push_back(path_str);
            }
        }
        final_files.insert(final_files.end(), opts.object_sources.begin(), opts.object_sources.end());

        bool show_filename = opts.with_filename || (final_files.size() > 1 && !opts.no_filename);
        long long total_matches = 0;

        if (final_files.empty()) {
            total_matches += processor.ProcessStream(std::cin, "(standard input)", opts.with_filename);
        } else {
            for (const auto& fname : final_files) {
                if (fname.rfind("registry:", 0) == 0 || fname.rfind("wmi:", 0) == 0) {
                    std::string content, error;
                    if (!GrepWindowsObjectLoader::LoadObjectInput(fname, content, error)) {
                        if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << error << "\n";
                        has_io_error = true;
                        continue;
                    }
                    std::istringstream object_stream(std::move(content));
                    total_matches += processor.ProcessStream(object_stream, fname, show_filename);
                    continue;
                }
                std::ifstream ifs(fname, std::ios::binary);
                if (!ifs.is_open()) {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << fname << ": No such file or directory\n";
                    has_io_error = true;
                    continue;
                }
                total_matches += processor.ProcessStream(ifs, fname, show_filename);
            }
        }

        if (has_io_error) {
            return 2;
        }

        return (total_matches > 0) ? 0 : 1;
    }
};
