#include "directory_traverser.hpp"
#include "file_wiper.hpp"
#include "pass_config.hpp"
#include "pass_generator.hpp"
#include "path_utils.hpp"
#include "user_prompt.hpp"
#include "wipe_app.hpp"
#include "wipe_options.hpp"

int WipeApplication::Run(int argc, char* argv[]) {
        WipeOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::vector<PassConfig> pass_configs = PassGenerator::GetPassConfigs(opts.passes, opts.dod_mode, opts.quick_mode);

        for (auto& target : opts.targets) {
            target = PathUtils::NormalizePath(target);

            DWORD attrs = GetFileAttributesA(target.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::cerr << "wipe: cannot access '" << target << "': No such file or directory\n";
                continue;
            }

            if (!opts.force && !opts.interactive) {
                if (!UserPrompt::ConfirmAction(target)) {
                    std::cout << "Skipped '" << target << "'.\n";
                    continue;
                }
            }

            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                if (!opts.recursive) {
                    std::cerr << "wipe: '" << target << "' is a directory (use -r to recurse)\n";
                    continue;
                }
                DirectoryTraverser::WipeDirectory(target, pass_configs, opts.recursive, opts.force, opts.verbose, opts.interactive);
            } else {
                FileWiper::WipeFile(target, pass_configs, opts.force, opts.verbose);
            }
        }

        return 0;
    }
