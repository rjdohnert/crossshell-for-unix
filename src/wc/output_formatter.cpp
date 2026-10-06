#include "counters.hpp"
#include "output_formatter.hpp"
#include "wc_options.hpp"

void OutputFormatter::PrintHeader(OutputFormat format) {
        if (format == OutputFormat::Csv) {
            std::cout << "\"file\",lines,words,chars,bytes,max_line_length\n";
        } else if (format == OutputFormat::Table) {
            std::cout << "FILE\tLINES\tWORDS\tCHARS\tBYTES\tMAX_LINE_LENGTH\n";
        }
    }

void OutputFormatter::PrintCounts(const Counters& c, const WcOptions& opts, const std::string& name) {
        if (opts.format == OutputFormat::Json) {
            std::cout << "{\"file\":\"" << name << "\",\"lines\":" << c.lines << ",\"words\":" << c.words
                      << ",\"chars\":" << c.chars << ",\"bytes\":" << c.bytes << ",\"max_line_length\":" << c.max_line_length << "}\n";
            return;
        }
        if (opts.format == OutputFormat::Csv) {
            std::cout << '"' << name << "\"," << c.lines << ',' << c.words << ',' << c.chars << ',' << c.bytes << ',' << c.max_line_length << "\n";
            return;
        }
        if (opts.format == OutputFormat::Table) {
            std::cout << name << "\t" << c.lines << "\t" << c.words << "\t" << c.chars << "\t" << c.bytes << "\t" << c.max_line_length << "\n";
            return;
        }
        if (opts.opt_lines) {
            std::cout << std::setw(8) << c.lines;
        }
        if (opts.opt_words) {
            std::cout << std::setw(8) << c.words;
        }
        if (opts.opt_chars) {
            std::cout << std::setw(8) << c.chars;
        }
        if (opts.opt_bytes) {
            std::cout << std::setw(8) << c.bytes;
        }
        if (opts.opt_max_len) {
            std::cout << std::setw(8) << c.max_line_length;
        }
        if (!name.empty()) {
            std::cout << " " << name;
        }
        std::cout << "\n";
    }
