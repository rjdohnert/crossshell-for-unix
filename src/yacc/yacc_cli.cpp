#include "bison_output_session.hpp"
#include "code_generator.hpp"
#include "grammar_helpers.hpp"
#include "grammar_spec.hpp"
#include "terminal_colors.hpp"
#include "yacc_cli.hpp"
#include "yacc_config.hpp"
#include "yacc_help.hpp"

int bison_main(int argc, char* argv[]) {
    EnableVT100Colors();

    try {

    if (argc < 2) {
        DisplayHelp();
        return 0;
    }

    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
            DisplayHelp();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            DisplayVersion();
            return 0;
        } else if (arg == "-d" || arg == "--defines") {
            config.generate_header = true;
        } else if (arg.rfind("--defines=", 0) == 0) {
            config.generate_header = true;
            config.header_file = arg.substr(10);
        } else if (arg == "-y" || arg == "--yacc") {
            config.yacc_mode = true;
            config.generate_header = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-t" || arg == "--debug") {
            config.debug_mode = true;
        } else if (arg == "-l" || arg == "--no-lines") {
            config.no_lines = true;
        } else if (arg == "--json" || arg == "-j") {
            config.output_format = Config::OutputFormat::Json;
        } else if (arg == "--csv") {
            config.output_format = Config::OutputFormat::Csv;
        } else if (arg == "--table") {
            config.output_format = Config::OutputFormat::Table;
        } else if (arg == "--output" && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") config.output_format = Config::OutputFormat::Json;
            else if (fmt == "csv") config.output_format = Config::OutputFormat::Csv;
            else if (fmt == "table") config.output_format = Config::OutputFormat::Table;
        } else if (arg == "--pipe" && i + 1 < argc) {
            config.pipe_command = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg.rfind("--output=", 0) == 0) {
            config.output_file = arg.substr(9);
        } else if (arg == "-p" && i + 1 < argc) {
            config.sym_prefix = argv[++i];
        } else if (arg.rfind("--prefix=", 0) == 0) {
            config.sym_prefix = arg.substr(9);
        } else if (arg == "-b" && i + 1 < argc) {
            config.file_prefix = argv[++i];
        } else if (arg.rfind("--file-prefix=", 0) == 0) {
            config.file_prefix = arg.substr(14);
        } else if (!arg.empty() && arg.front() != '-') {
            config.input_file = arg;
        }
    }

    if (config.input_file.empty()) {
        std::cerr << Color::RED << "[!] Error: No input grammar file specified.\n"
                  << "    Run 'yacc --help' for usage guidance." << Color::RESET << std::endl;
        return 1;
    }

    BisonOutputSession output_session(config.output_format, config.pipe_command);

    GrammarSpec grammar;
    std::string parse_err;

    std::cout << Color::BOLD << Color::CYAN << "[+] Processing grammar file: " << config.input_file << Color::RESET << std::endl;

    if (!grammar.Parse(config.input_file, parse_err)) {
        std::cerr << Color::RED << "[!] Syntax Error: " << parse_err << Color::RESET << std::endl;
        return 1;
    }

    std::string gen_err;
    if (!CodeGenerator::Generate(config, grammar, gen_err)) {
        std::cerr << Color::RED << "[!] Code Generation Error: " << gen_err << Color::RESET << std::endl;
        return 1;
    }

    std::cout << Color::BOLD << Color::GREEN << "[+] LALR(1) Parser generated (" 
              << grammar.states.size() << " states)" << Color::RESET << std::endl;

    if (grammar.shift_reduce_conflicts > 0 || grammar.reduce_reduce_conflicts > 0) {
        std::cout << Color::YELLOW << "[!] Warnings during state generation:\n";
        if (grammar.shift_reduce_conflicts > 0)
            std::cout << "  -> " << grammar.shift_reduce_conflicts << " shift/reduce conflict(s)\n";
        if (grammar.reduce_reduce_conflicts > 0)
            std::cout << "  -> " << grammar.reduce_reduce_conflicts << " reduce/reduce conflict(s)\n";
        std::cout << Color::RESET;
    }

    if (config.generate_header) {
        std::string hname = config.header_file.empty() ? (config.yacc_mode ? "y.tab.h" : GetBaseFilename(config.input_file) + ".tab.h") : config.header_file;
        std::cout << "  -> Header file: " << Color::YELLOW << hname << Color::RESET << "\n";
    }
    std::string cname = config.output_file.empty() ? (config.yacc_mode ? "y.tab.c" : GetBaseFilename(config.input_file) + ".tab.c") : config.output_file;
    std::cout << "  -> Source file: " << Color::YELLOW << cname << Color::RESET << "\n";

    if (config.verbose) {
        std::string vname = config.verbose_file.empty() ? GetBaseFilename(config.input_file) + ".output" : config.verbose_file;
        std::cout << "  -> Report file: " << Color::YELLOW << vname << Color::RESET << "\n";
    }

    return 0;
    } catch (const std::exception& e) {
        std::cerr << Color::RED << "[!] Fatal error: " << e.what() << Color::RESET << std::endl;
        return 1;
    } catch (...) {
        std::cerr << Color::RED << "[!] Fatal error: unexpected exception" << Color::RESET << std::endl;
        return 1;
    }
}
