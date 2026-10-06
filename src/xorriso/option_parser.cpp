#include "option_parser.hpp"
#include "xorriso_config.hpp"

void OptionParser::PrintHeader() {
        std::cout << "Xorriso v5.1.0 - ISO Utility\n";
        std::cout << "Copyright (C) 2026. Roberto J. Dohnert All rights reserved.\n";
        std::cout << "-------------------------------------------------------------------\n";
    }

void OptionParser::PrintHelp() {
        PrintHeader();
        std::cout << R"(
USAGE EXAMPLES:
  1. Build an ISO Image (mkisofs compatibility mode):
     xorriso.exe -as mkisofs -o C:\output.iso -V "MY_LABEL" C:\source_folder

  2. High-Speed Direct Unbuffered ISO Extraction (-osirrox):
     xorriso.exe -osirrox -extract C:\input.iso C:\target_folder

  3. Inspect ISO Volume Header & Contents:
     xorriso.exe -indev C:\input.iso -toc

COMMAND REFERENCE:
  -help, /?                Display this comprehensive help screen.
  -as mkisofs              Emulate classic mkisofs/genisoimage command flags.
  -o <path>                Specify output ISO file destination.
  -V <label>               Set Volume Identifier (Volume Label, max 32 chars).
  -osirrox                 Enable filesystem extraction mode.
  -extract <iso> <dir>     Extract contents from <iso> into local directory <dir>.
  -indev <iso> -toc        Inspect Volume Table of Contents & PVD metadata.
  -v                       Enable verbose kernel console diagnostics.
)";
    }

bool OptionParser::Parse(int argc, char* argv[], XorrisoConfig& cfg, bool& exitEarly) const {
        exitEarly = false;
        if (argc < 2) {
            PrintHelp();
            exitEarly = true;
            return true;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-help" || arg == "/?" || arg == "--help") {
                PrintHelp();
                exitEarly = true;
                return true;
            } else if (arg == "-v") {
                cfg.verbose = true;
            } else if (arg == "-as" && i + 1 < argc && std::string(argv[i + 1]) == "mkisofs") {
                cfg.mode = EngineMode::Mkisofs;
                i++;
            } else if (arg == "-o" && i + 1 < argc) {
                cfg.isoPath = fs::path(argv[++i]).wstring();
            } else if (arg == "-V" && i + 1 < argc) {
                cfg.volumeID = argv[++i];
            } else if (arg == "-osirrox") {
                cfg.mode = EngineMode::Extract;
            } else if (arg == "-extract" && i + 2 < argc) {
                cfg.mode = EngineMode::Extract;
                cfg.isoPath = fs::path(argv[++i]).wstring();
                cfg.extractDir = fs::path(argv[++i]).wstring();
            } else if (arg == "-indev" && i + 1 < argc) {
                cfg.mode = EngineMode::Inspect;
                cfg.isoPath = fs::path(argv[++i]).wstring();
            } else if (arg == "-toc") {
                if (cfg.mode != EngineMode::Inspect) cfg.mode = EngineMode::Inspect;
            } else if (cfg.sourceDir.empty() && arg[0] != '-') {
                cfg.sourceDir = fs::path(arg).wstring();
            }
        }
        return true;
    }
