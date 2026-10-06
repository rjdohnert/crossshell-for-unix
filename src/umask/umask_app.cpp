#include "system_umask_manager.hpp"
#include "umask_app.hpp"
#include "umask_formatter.hpp"
#include "umask_options.hpp"
#include "umask_parser.hpp"

int UmaskApplication::Run(int argc, char* argv[]) const {
        UmaskOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.show_help) {
            options.PrintHelp();
            return 0;
        }

        unsigned int current_mask = SystemUmaskManager::GetSystemUmask();

        if (!options.new_mask_arg.empty()) {
            try {
                unsigned int new_mask = UmaskParser::ParseMaskInput(current_mask, options.new_mask_arg);
                SystemUmaskManager::SetSystemUmask(new_mask);
                current_mask = new_mask;

                if (options.subcommand_str.empty()) {
                    std::cout << "umask: mask set to " << UmaskFormatter::FormatOctalMask(current_mask) 
                              << " (" << UmaskFormatter::FormatSymbolicMask(current_mask) << ")\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "umask: " << e.what() << "\n";
                return 1;
            }
        }

        if (!options.subcommand_str.empty()) {
            std::cout << "umask: running subcommand with mask " << UmaskFormatter::FormatOctalMask(current_mask) << "...\n";
            return SystemUmaskManager::ExecuteSubcommand(options.subcommand_str);
        }

        if (options.new_mask_arg.empty() || options.flag_print_reusable) {
            if (options.flag_print_reusable) {
                if (options.flag_symbolic) {
                    std::cout << "umask -S " << UmaskFormatter::FormatSymbolicMask(current_mask) << "\n";
                } else {
                    std::cout << "umask " << UmaskFormatter::FormatOctalMask(current_mask) << "\n";
                }
            } else if (options.flag_symbolic) {
                std::cout << UmaskFormatter::FormatSymbolicMask(current_mask) << "\n";
            } else {
                std::cout << UmaskFormatter::FormatOctalMask(current_mask) << "\n";
            }
        }

        return 0;
    }
