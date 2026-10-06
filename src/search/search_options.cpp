#include "search_options.hpp"
#include "terminal_format.hpp"
#include "known_folders.hpp"

void OptionParser::PrintHelp() {
    std::cout <<
R"(search(1)                 CrossShell for UNIX Reference Manual                  search(1)

NAME
    search - advanced recursive file and metadata search engine for Windows

SYNOPSIS
    search [PATHS...] [NAME_PATTERN] [OPTIONS]

DESCRIPTION
    Recursively searches files and directories matching names, wildcards,
    regular expressions, file extensions, sizes, timestamps, and Windows
    file attributes. If no target directory is specified, the current
    directory (".") is searched.

OPTIONS
    Path and User Directory:
        [PATHS...]
            One or more directories or drives to search (e.g., ".", "C:\").
        -u, --user
            Search user personal directories (Downloads, Documents, Desktop,
            Pictures, Music, Videos, including OneDrive folders).

    Name and Pattern Matching:
        -n, --name PATTERN
            Search pattern with wildcards ('*' and '?').
        -r, --regex REGEX
            Search name using ECMAScript regular expressions.
        -e, --ext EXTS
            Comma-separated list of extensions (e.g., "cpp,h", "pdf").
        -x, --exact
            Enforce exact full-name matching instead of substring.
        -s, --case-sensitive
            Enable case-sensitive matching (default: case-insensitive).

    Metadata and Size Filters:
        -t, --type TYPE
            Filter entry type: (f)ile, (d)irectory, (a)ll.
        --min-size SIZE
            Find files >= size (e.g., 512B, 100KB, 50MB, 2.5GB, 1TB).
        --max-size SIZE
            Find files <= size.
        --empty
            Find empty files (0 bytes) or empty directories.

    Date and Time Filters:
        --after DATE
            Modified on/after date (YYYY-MM-DD) or relative duration.
        --before DATE
            Modified on/before date (YYYY-MM-DD).
        --newer-than DURATION
            Modified within duration (e.g. "30m", "12h", "7d", "30d").
        --older-than DURATION
            Modified prior to duration.

    Windows Attributes and Traversal:
        -d, --depth N
            Limit recursion depth (0 = target directory only).
        --hidden
            Match hidden files and directories only.
        --no-hidden
            Skip hidden files and directories.
        --readonly
            Match read-only files only.
        --system
            Match system files only.

    Output and Display:
        -b, --bare
            Print only matching file paths (ideal for piping).
        --summary
            Print summary statistics only.
        -h, --help, /?
            Display this comprehensive reference manual and exit.

EXAMPLES
    search -u "invoice*.pdf"
        Find downloads matching pattern across user directories.

    search budget
        Find file or folder named 'budget' in current dir and subdirs.

    search --user --min-size 100MB --newer-than 7d
        Search user folders for large files modified this week.

    search C:\Engine D:\Plugins -e cpp,h,hpp
        Find all C++ source and header files in two project folders.

    search D:\Logs -n "*.log" --before 2024-01-01
        Find all log files modified before 2024-01-01.

    search C:\Config -e json,ini --readonly --bare > config.txt
        Output bare paths of read-only configs to a file.

EXIT STATUS
    0   Success.
    1   Invalid options or search execution failure.

    CrossShell for UNIX                                                 search(1)
)";
}

bool OptionParser::Parse(int argc, char* argv[], SearchOptions& opt) const {
    std::vector<std::string> positionalArgs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return false;
        } else if (arg == "-u" || arg == "--user") {
            opt.searchUserDirs = true;
        } else if ((arg == "-n" || arg == "--name") && i + 1 < argc) {
            opt.namePattern = argv[++i];
            opt.isRegex = false;
        } else if ((arg == "-r" || arg == "--regex") && i + 1 < argc) {
            opt.namePattern = argv[++i];
            opt.isRegex = true;
        } else if ((arg == "-e" || arg == "--ext") && i + 1 < argc) {
            std::string extList = argv[++i];
            std::stringstream ss(extList);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    if (item[0] != '.') item = "." + item;
                    std::transform(item.begin(), item.end(), item.begin(), ::tolower);
                    opt.extensions.push_back(item);
                }
            }
        } else if ((arg == "-t" || arg == "--type") && i + 1 < argc) {
            std::string val = argv[++i];
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(val[0])));
            if (c == 'f' || c == 'd' || c == 'a') opt.typeFilter = c;
            else {
                std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid type filter: " << val << " (Use f, d, or all)\n";
                return false;
            }
        } else if (arg == "-x" || arg == "--exact") {
            opt.exactMatch = true;
        } else if (arg == "-s" || arg == "--case-sensitive") {
            opt.caseInsensitive = false;
        } else if ((arg == "-d" || arg == "--depth") && i + 1 < argc) {
            opt.maxDepth = std::stoi(argv[++i]);
        } else if (arg == "--min-size" && i + 1 < argc) {
            opt.minSize = SizeFormatter::Parse(argv[++i]);
            if (!opt.minSize) {
                std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid size format: " << argv[i] << "\n";
                return false;
            }
        } else if (arg == "--max-size" && i + 1 < argc) {
            opt.maxSize = SizeFormatter::Parse(argv[++i]);
            if (!opt.maxSize) {
                std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid size format: " << argv[i] << "\n";
                return false;
            }
        } else if (arg == "--empty") {
            opt.minSize = 0;
            opt.maxSize = 0;
        } else if ((arg == "--after" || arg == "--newer-than") && i + 1 < argc) {
            opt.modifiedAfter = DateTimeFormatter::Parse(argv[++i]);
            if (!opt.modifiedAfter) {
                std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid date/duration: " << argv[i] << "\n";
                return false;
            }
        } else if ((arg == "--before" || arg == "--older-than") && i + 1 < argc) {
            opt.modifiedBefore = DateTimeFormatter::Parse(argv[++i]);
            if (!opt.modifiedBefore) {
                std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid date/duration: " << argv[i] << "\n";
                return false;
            }
        } else if (arg == "--hidden") {
            opt.hiddenOnly = true;
        } else if (arg == "--no-hidden") {
            opt.includeHidden = false;
        } else if (arg == "--readonly") {
            opt.readonlyOnly = true;
        } else if (arg == "--system") {
            opt.systemOnly = true;
        } else if (arg == "-b" || arg == "--bare") {
            opt.bareOutput = true;
        } else if (arg == "--summary") {
            opt.summaryOnly = true;
        } else if (arg[0] != '-') {
            positionalArgs.push_back(arg);
        } else {
            std::cerr << ConsoleTerminal::Red << "Unknown option: " << ConsoleTerminal::Reset << arg 
                      << " (Run " << ConsoleTerminal::Yellow << "search --help" << ConsoleTerminal::Reset << " for syntax)\n";
            return false;
        }
    }

    if (opt.searchUserDirs) {
        KnownFolderLocator::PopulateUserCommonPaths(opt.targetPaths);
    }

    for (const auto& pos : positionalArgs) {
        std::error_code ec;
        if (fs::exists(pos, ec)) {
            opt.targetPaths.push_back(fs::path(pos));
        } else {
            if (opt.namePattern.empty()) {
                opt.namePattern = pos;
            } else {
                opt.targetPaths.push_back(fs::path(pos));
            }
        }
    }

    if (opt.targetPaths.empty()) {
        opt.targetPaths.push_back(fs::current_path());
    }

    return true;
}
