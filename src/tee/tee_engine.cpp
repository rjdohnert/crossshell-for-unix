#include "output_formatter.hpp"
#include "tee_engine.hpp"
#include "tee_options.hpp"

bool TeeEngine::Process(const TeeOptions& opts) {
        FILE* outputPipe = nullptr;
        if (!opts.pipe_command.empty()) {
            outputPipe = _wpopen(opts.pipe_command.c_str(), L"w");
        }
        std::string structuredInput;

        if (opts.ignore_interrupts) {
#ifdef _WIN32
            SetConsoleCtrlHandler(nullptr, TRUE);
#endif
        }

        std::vector<std::ofstream> streams;
        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (opts.append) {
            mode |= std::ios::app;
        }

        bool had_error = false;
        for (const auto& path_str : opts.file_paths) {
            fs::path p(path_str);
            std::ofstream stream(p, mode);
            if (!stream) {
                std::wcerr << L"tee: " << path_str << L": Failed to open file\n";
                had_error = true;
            } else {
                streams.push_back(std::move(stream));
            }
        }

        char buffer[16384];
        while (std::cin) {
            std::cin.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = std::cin.gcount();
            if (bytes_read > 0) {
                if (opts.output_format == 0 && !outputPipe && !std::cout.write(buffer, bytes_read)) {
                    had_error = true;
                }
                if (opts.output_format != 0 || outputPipe) {
                    structuredInput.append(buffer, static_cast<size_t>(bytes_read));
                }
                std::cout.flush();

                for (auto& fs_stream : streams) {
                    if (fs_stream.is_open()) {
                        if (!fs_stream.write(buffer, bytes_read)) {
                            had_error = true;
                        }
                        fs_stream.flush();
                    }
                }
            }
        }

        OutputFormatter::Emit(opts.output_format, outputPipe, structuredInput);

        if (outputPipe) {
            _pclose(outputPipe);
        }

        return !had_error;
    }
