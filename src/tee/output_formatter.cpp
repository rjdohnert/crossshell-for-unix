#include "output_formatter.hpp"

void OutputFormatter::Emit(int format, FILE* outputPipe, const std::string& data) {
        if (format == 0 && !outputPipe) return;

        std::string text;
        if (format == 1) {
            text = "{\"data\":\"" + data + "\"}\n";
        } else if (format == 2) {
            text = "\"data\"\n\"" + data + "\"\n";
        } else {
            text = "DATA\n----\n" + data;
        }

        if (outputPipe) {
            fwrite(text.data(), 1, text.size(), outputPipe);
        } else {
            std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
        }
    }
