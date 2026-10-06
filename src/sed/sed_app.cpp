#include "sed_app.hpp"

int SedApplication::Run(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    SedOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        return 2;
    }
    if (exitEarly) {
        return 0;
    }

    opts.files.insert(opts.files.end(), opts.object_sources.begin(), opts.object_sources.end());

    std::vector<SedCommand> compiled_commands;
    for (const auto& s : opts.scripts) {
        auto cmds = m_scriptParser.parse(s);
        compiled_commands.insert(compiled_commands.end(), cmds.begin(), cmds.end());
    }

    SedEngine engine(compiled_commands, opts);

    if (opts.files.empty()) {
        engine.ProcessStream(std::cin, std::cout);
    } else {
        for (const auto& filepath : opts.files) {
            if (filepath.rfind("registry:", 0) == 0 || filepath.rfind("wmi:", 0) == 0) {
                std::string content, error;
                if (!SedWindowsObjectLoader::LoadObjectInput(filepath, content, error)) {
                    std::cerr << "sed: " << error << "\n";
                    continue;
                }
                std::istringstream input(std::move(content));
                engine.ProcessStream(input, std::cout);
                continue;
            }
            if (opts.in_place) {
                std::error_code ec;
                fs::file_status st = fs::status(filepath, ec);
                if (ec || !fs::exists(st) || !fs::is_regular_file(st)) {
                    std::cerr << "sed: cannot operate on non-regular file " << filepath << "\n";
                    continue;
                }
                if (fs::symlink_status(filepath, ec).type() == fs::file_type::symlink) {
                    if (!opts.follow_symlinks) {
                        std::cerr << "sed: refusing to modify symlink " << filepath << "\n";
                        continue;
                    }
                }

                std::ifstream in(filepath, std::ios::binary);
                if (!in.is_open()) {
                    std::cerr << "sed: cannot open file " << filepath << "\n";
                    continue;
                }

                std::string temp_path = filepath + ".tmp." + std::to_string(std::hash<std::string>{}(filepath));
                std::ofstream out(temp_path, std::ios::binary);
                if (!out.is_open()) {
                    std::cerr << "sed: cannot create temporary file for " << filepath << "\n";
                    continue;
                }

                engine.ProcessStream(in, out);
                in.close();
                out.close();

                if (!opts.backup_suffix.empty()) {
                    std::string backup_path = filepath + opts.backup_suffix;
                    fs::copy_file(filepath, backup_path, fs::copy_options::overwrite_existing, ec);
                    if (ec) {
                        std::cerr << "sed: cannot create backup file " << backup_path << ": " << ec.message() << "\n";
                    }
                }
                ec.clear();
                fs::copy_file(temp_path, filepath, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    std::cerr << "sed: cannot overwrite " << filepath << ": " << ec.message() << "\n";
                }
                fs::remove(temp_path, ec);
            } else {
                std::ifstream in(filepath, std::ios::binary);
                if (!in.is_open()) {
                    std::cerr << "sed: cannot open file " << filepath << "\n";
                    continue;
                }
                engine.ProcessStream(in, std::cout);
            }
        }
    }

    return 0;
}
