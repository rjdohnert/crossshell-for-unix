#include "iso_builder.hpp"
#include "iso_extractor.hpp"
#include "iso_inspector.hpp"
#include "option_parser.hpp"
#include "xorriso_app.hpp"
#include "xorriso_config.hpp"

int XorrisoApplication::Run(int argc, char* argv[]) {
        XorrisoConfig cfg;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, cfg, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool result = false;

        switch (cfg.mode) {
            case EngineMode::Mkisofs:
                if (cfg.isoPath.empty() || cfg.sourceDir.empty()) {
                    std::cerr << "[-] Error: Missing arguments for ISO creation (-o <out.iso> <source_dir>)\n";
                    return 1;
                }
                result = IsoBuilder::Build(cfg);
                break;

            case EngineMode::Extract:
                if (cfg.isoPath.empty() || cfg.extractDir.empty()) {
                    std::cerr << "[-] Error: Missing extraction parameters (-extract <in.iso> <out_dir>)\n";
                    return 1;
                }
                result = IsoExtractor::Extract(cfg);
                break;

            case EngineMode::Inspect:
                if (cfg.isoPath.empty()) {
                    std::cerr << "[-] Error: Missing ISO path (-indev <image.iso>)\n";
                    return 1;
                }
                result = IsoInspector::Inspect(cfg);
                break;

            default:
                OptionParser::PrintHelp();
                return 0;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = endTime - startTime;

        if (result) {
            std::cout << "[+] Operation completed successfully in " << duration.count() << " ms.\n";
            return 0;
        } else {
            std::cerr << "[-] Operation failed.\n";
            return 1;
        }
    }
