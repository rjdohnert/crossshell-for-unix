#include "options.hpp"

void OptionParser::DisplayHelp() {
    std::cout <<
R"(sed(1)                CrossShell for UNIX Reference Manual                sed(1)

NAME
    sed - stream editor for filtering and transforming text

SYNOPSIS
    sed [OPTIONS] SCRIPT [FILE...]
    sed [OPTIONS] -e SCRIPT... [FILE...]
    <input-stream> | sed [OPTIONS] SCRIPT

DESCRIPTION
    sed is a stream editor used to perform basic text and structured object
    transformations on an input stream (a file or input from a pipeline).
    It natively understands both line-oriented text stream transformations
    and JSON/Object manipulations (nested mutation, key deletion, and field
    substitution).

OPTIONS
    -e, --expression=SCRIPT
        Add script commands to the execution pipeline. Multiple -e arguments
        can be chained together or separated by ';'.
    -f, --file=SCRIPT_FILE
        Read transformation commands directly from a script file.
    -i[SUFFIX], --in-place[=SUFFIX]
        Edit files in-place (overwriting original). If SUFFIX is supplied
        (e.g., -i.bak), creates a backup of the original file.
    -n, --quiet, --silent
        Suppress automatic printing of pattern space. Output is produced
        only when explicitly requested via the 'p' command or 'p' flag.
    -0, --null-data
        Separate records by NUL characters (\0) instead of newlines.
    -j, --json-only
        Force pure JSON mode. Non-JSON records are skipped.
    -t, --text-only
        Disable JSON object auto-detection. Treat all inputs as pure text.
    -E, -r, --regexp-extended
        Use extended regular expression syntax.
    -s, --separate
        Treat each input file as a separate stream for addressing.
    -u, --unbuffered
        Flush output after each processed record.
    -b, --binary
        Read and write input as binary data where supported.
    --posix
        Select POSIX compatibility behavior.
    --sandbox
        Select restricted execution behavior.
    --debug
        Enable diagnostic processing output.
    --follow-symlinks
        Follow symlinks when performing in-place edits.
    --registry KEY
        Read registry values as JSON Lines before applying the script.
    --wmi QUERY[|NAMESPACE]
        Read WMI objects as JSON Lines (default namespace ROOT\CIMV2).
    -h, --help
        Display this comprehensive reference manual and exit.
    -V, --version
        Display version and environment information and exit.

COMMAND SYNTAX
    Plain Text Substitution:
        s/regexp/replacement/[flags]
        s#regexp#replacement#[flags]
        Flags:
            g   Replace globally (all occurrences in line/field).
            i   Case-insensitive regular expression match.
            p   Print the pattern space if a substitution was made.

    Field-Scoped Object Substitution:
        s/.path.to.key/regexp/replacement/[flags]
        Performs regular expression substitution exclusively within the target
        nested JSON object or array field.

    Object Property Mutations:
        set .path.to.key = <value>
            Sets or adds a nested field in a JSON record.
        del .path.to.key
            Deletes a specific key from a JSON object or array item.

    Record Flow and Control:
        d   Delete line/record (prevents it from being printed).
        p   Print the current pattern space immediately.
        q   Quit sed immediately (stops reading further input).

ADDRESSING
    Commands can be prefixed with address conditions:
        3d                           Delete line 3.
        1,5s/foo/bar/g               Substitute 'foo' with 'bar' on lines 1 to 5.
        /ERROR/d                     Delete all lines matching regex 'ERROR'.
        .status >= 500:d             Delete JSON records where status >= 500.
        .env == "prod":set .debug=0  Set debug to 0 on records where env is 'prod'.

EXAMPLES
    sed "s/http:\/\//https:\/\//g" urls.txt
        Replace http with https across input text file.

    sed -i.bak "s/DEBUG/INFO/g" server.log
        In-place replace with backup file creation.

    sed ".user.id == 101:set .user.role = \"admin\"" users.jsonl
        Mutate nested JSON property conditionally.

    sed "s/.user.email/@.*$/@redacted.com/g" accounts.jsonl
        Redact email domain within JSON object stream.

    sed "del .meta.trace_id; del .meta.debug" telemetry.jsonl
        Delete metadata fields from JSON stream.

EXIT STATUS
    0   Success.
    1   Syntax error, unknown command, or execution failure.

    CrossShell for UNIX                                                   sed(1)
)";
}

void OptionParser::DisplayVersion() {
    std::cout << "sed version 2.0.0\n"
              << "Copyright (C) 2026, Roberto J Dohnert\n";
}

bool OptionParser::Parse(int argc, char* argv[], SedOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    std::string script_arg = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (opts.end_of_options) {
            if (opts.scripts.empty() && script_arg.empty()) script_arg = arg;
            else opts.files.push_back(arg);
            continue;
        }
        if (arg == "--") {
            opts.end_of_options = true;
            continue;
        }

        if (arg == "-h" || arg == "--help") { DisplayHelp(); exitEarly = true; return true; }
        if (arg == "-V" || arg == "--version") { DisplayVersion(); exitEarly = true; return true; }
        else if (arg == "-n" || arg == "--quiet" || arg == "--silent") opts.quiet = true;
        else if (arg == "-E" || arg == "-r" || arg == "--regexp-extended") opts.extended_regex = true;
        else if (arg == "-s" || arg == "--separate") opts.separate_files = true;
        else if (arg == "-u" || arg == "--unbuffered") opts.unbuffered = true;
        else if (arg == "-b" || arg == "--binary") opts.binary_mode = true;
        else if (arg == "--posix") opts.posix_mode = true;
        else if (arg == "--sandbox") opts.sandbox_mode = true;
        else if (arg == "--debug") opts.debug_mode = true;
        else if (arg == "--follow-symlinks") opts.follow_symlinks = true;
        else if (arg == "-j" || arg == "--json-only") opts.json_only = true;
        else if (arg == "-t" || arg == "--text-only") opts.text_only = true;
        else if (arg == "-0" || arg == "--null-data") opts.record_delim = '\0';
        else if (arg == "-i") {
            opts.in_place = true;
            if (i + 1 < argc && argv[i + 1][0] == '.') {
                opts.backup_suffix = argv[++i];
            }
        } else if (arg.rfind("-i", 0) == 0) {
            opts.in_place = true;
            opts.backup_suffix = arg.substr(2);
        } else if (arg.rfind("--in-place", 0) == 0) {
            opts.in_place = true;
            if (arg.find('=') != std::string::npos) {
                opts.backup_suffix = arg.substr(arg.find('=') + 1);
            } else if (i + 1 < argc && argv[i + 1][0] == '.') {
                opts.backup_suffix = argv[++i];
            }
        } else if ((arg == "-e" || arg == "--expression") && i + 1 < argc) {
            opts.scripts.push_back(argv[++i]);
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            std::ifstream sf(argv[++i]);
            if (!sf.is_open()) {
                std::cerr << "sed: cannot read script file " << argv[i] << "\n";
                return false;
            }
            std::string content((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
            opts.scripts.push_back(content);
        } else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) {
            opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
        } else if (!arg.empty() && arg[0] != '-') {
            if (opts.scripts.empty() && script_arg.empty()) {
                script_arg = arg;
                if (script_arg.rfind("set ", 0) == 0) {
                    while (i + 1 < argc && (script_arg.find('=') == std::string::npos || script_arg.back() == '=' || script_arg.back() == ' ')) {
                        script_arg += " " + std::string(argv[++i]);
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }
    }

    if (!script_arg.empty()) {
        opts.scripts.push_back(script_arg);
    }

    if (opts.scripts.empty()) {
        std::cerr << "sed: no script or expression specified.\nTry 'sed --help' for more information.\n";
        return false;
    }

    return true;
}
