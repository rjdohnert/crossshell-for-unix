#include "pipe_stream_handler.hpp"

void PipeStreamHandler::configureBinaryMode() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    }

std::vector<uint8_t> PipeStreamHandler::readFromStdin() {
        configureBinaryMode();
        std::vector<uint8_t> buffer;
        char chunk[16384];
        while (std::cin.read(chunk, sizeof(chunk)) || std::cin.gcount() > 0) {
            buffer.insert(buffer.end(), chunk, chunk + std::cin.gcount());
        }
        return buffer;
    }

void PipeStreamHandler::writeToStdout(const std::vector<uint8_t>& data) {
        configureBinaryMode();
        std::cout.write(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout.flush();
    }
