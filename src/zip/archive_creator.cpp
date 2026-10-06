#include "archive_creator.hpp"
#include "compression_level.hpp"
#include "output_session.hpp"
#include "powershell_quoting.hpp"
#include "zip_options.hpp"

int create_zip_archive(const ZipOptions& opts) {
    OutputSession output_session(opts.output_format, opts.pipe_command);

    std::vector<fs::path> archive_inputs;
    std::vector<fs::path> files_for_junk_mode;
    size_t files_added = 0;

    for (const auto& input_str : opts.input_paths) {
        fs::path p(input_str);

        bool excluded = false;
        for (const auto& ex : opts.exclude_paths) {
            if (p.string() == ex || p.filename().string() == ex) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;

        if (!fs::exists(p)) {
            std::cerr << "zip warning: name not matched: " << input_str << "\n";
            continue;
        }

        if (fs::is_directory(p)) {
            if (!opts.recursive) {
                std::cerr << "zip warning: " << input_str << " is a directory (use -r to recurse)\n";
                continue;
            }

            archive_inputs.push_back(p);
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) {
                    files_added++;
                    files_for_junk_mode.push_back(entry.path());
                }
            }
            continue;
        }

        if (fs::is_regular_file(p)) {
            archive_inputs.push_back(p);
            files_for_junk_mode.push_back(p);
            files_added++;
        }
    }

    if (archive_inputs.empty() || files_added == 0) {
        std::cerr << "zip error: Nothing to do!\n";
        return 1;
    }

    fs::path temp_stage_dir;
    std::vector<fs::path> ps_inputs;

    if (opts.junk_paths) {
        temp_stage_dir = fs::temp_directory_path() /
            (std::string("cmdext-zip-stage-") + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));

        try {
            fs::create_directories(temp_stage_dir);

            for (const auto& src : files_for_junk_mode) {
                fs::path candidate = temp_stage_dir / src.filename();
                int idx = 1;
                while (fs::exists(candidate)) {
                    candidate = temp_stage_dir /
                        (src.stem().string() + "_" + std::to_string(idx++) + src.extension().string());
                }
                fs::copy_file(src, candidate, fs::copy_options::overwrite_existing);
                ps_inputs.push_back(candidate);

                if (!opts.quiet) {
                    std::cout << "  adding: " << candidate.filename().string();
                    if (opts.compression_level == 0) {
                        std::cout << " (stored)\n";
                    } else {
                        std::cout << " (deflated)\n";
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "zip error: Failed preparing junk-path staging area: " << ex.what() << "\n";
            if (!temp_stage_dir.empty()) {
                std::error_code ec;
                fs::remove_all(temp_stage_dir, ec);
            }
            return 1;
        }
    } else {
        ps_inputs = archive_inputs;
        if (!opts.quiet) {
            for (const auto& in : ps_inputs) {
                std::cout << "  adding: " << in.string();
                if (opts.compression_level == 0) {
                    std::cout << " (stored)\n";
                } else {
                    std::cout << " (deflated)\n";
                }
            }
        }
    }

    std::ostringstream path_array;
    path_array << "@(";
    for (size_t i = 0; i < ps_inputs.size(); ++i) {
        if (i != 0) path_array << ",";
        path_array << ps_quote(ps_inputs[i].string());
    }
    path_array << ")";

    std::string psCommand =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Compress-Archive -Path " + path_array.str() +
        " -DestinationPath " + ps_quote(fs::absolute(opts.zip_filename).string()) +
        " -CompressionLevel " + compression_level_to_ps(opts.compression_level) +
        " -Force\"";

    int rc = std::system(psCommand.c_str());

    if (!temp_stage_dir.empty()) {
        std::error_code ec;
        fs::remove_all(temp_stage_dir, ec);
    }

    if (rc != 0) {
        std::cerr << "zip error: Failed to create archive via PowerShell Compress-Archive.\n";
        return 1;
    }

    if (!opts.quiet) {
        std::cout << "Created " << opts.zip_filename << " (" << files_added << " file(s) added)\n";
    }

    return 0;
}
