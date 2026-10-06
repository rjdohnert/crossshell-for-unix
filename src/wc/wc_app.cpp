#include "counters.hpp"
#include "output_formatter.hpp"
#include "pipe_session.hpp"
#include "text_metrics_scanner.hpp"
#include "wc_app.hpp"
#include "wc_options.hpp"

int WcApplication::Run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif

        WcOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        PipeSession pipe_session(opts.pipe_command);
        OutputFormatter::PrintHeader(opts.format);

        Counters total;
        bool success = true;
        int processed_inputs = 0;

        for (const auto& file : opts.files) {
            Counters file_c;
            if (file == "-") {
                TextMetricsScanner::ScanStream(std::cin, file_c);
                OutputFormatter::PrintCounts(file_c, opts, opts.implicit_stdin ? "" : "-");
                processed_inputs++;
            } else {
                std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
                if (!infile.is_open()) {
                    std::cerr << "wc: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                TextMetricsScanner::ScanStream(infile, file_c);
                OutputFormatter::PrintCounts(file_c, opts, file);
                processed_inputs++;
            }

            total.lines += file_c.lines;
            total.words += file_c.words;
            total.chars += file_c.chars;
            total.bytes += file_c.bytes;
            if (file_c.max_line_length > total.max_line_length) {
                total.max_line_length = file_c.max_line_length;
            }
        }

        if (processed_inputs > 1) {
            OutputFormatter::PrintCounts(total, opts, "total");
        }

        return success ? 0 : 1;
    }
