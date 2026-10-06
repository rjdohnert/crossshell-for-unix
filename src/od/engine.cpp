#include "engine.hpp"

const char* OdFormatter::ASCII_NAMES[128] = {
    "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
    "bs",  "ht",  "nl",  "vt",  "ff",  "cr",  "so",  "si",
    "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
    "can", "em",  "sub", "esc", "fs",  "gs",  "rs",  "us",
    "sp",  "!",   "\"",  "#",   "$",   "%",   "&",   "'",
    "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",
    "`",   "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   "del"
};

void OdFormatter::printAddress(std::ostream& out, uint64_t addr, char radix) {
    if (radix == 'n') return;
    std::ios state(nullptr);
    state.copyfmt(out);

    if (radix == 'o') {
        out << std::setfill('0') << std::setw(7) << std::oct << addr << " ";
    } else if (radix == 'x') {
        out << std::setfill('0') << std::setw(6) << std::hex << addr << " ";
    } else if (radix == 'd') {
        out << std::setfill('0') << std::setw(7) << std::dec << addr << " ";
    }
    out.copyfmt(state);
}

void OdFormatter::formatChunk(std::ostream& out, const uint8_t* data, size_t size, const FormatSpec& fmt) {
    std::ios state(nullptr);
    state.copyfmt(out);

    for (size_t i = 0; i < size; i += fmt.size) {
        size_t available = (i + fmt.size <= size) ? fmt.size : (size - i);

        if (fmt.kind == FormatKind::ASCII_NAMED) {
            uint8_t byte = data[i] & 0x7F;
            out << std::setw(3) << ASCII_NAMES[byte] << " ";
        } else if (fmt.kind == FormatKind::CHAR_ESCAPE) {
            char c = static_cast<char>(data[i]);
            if (c == '\0') out << "  \\0";
            else if (c == '\a') out << "  \\a";
            else if (c == '\b') out << "  \\b";
            else if (c == '\f') out << "  \\f";
            else if (c == '\n') out << "  \\n";
            else if (c == '\r') out << "  \\r";
            else if (c == '\t') out << "  \\t";
            else if (c == '\v') out << "  \\v";
            else if (std::isprint(static_cast<unsigned char>(c))) out << "   " << c;
            else {
                out << " " << std::setfill('0') << std::setw(3) << std::oct << static_cast<int>(static_cast<unsigned char>(c));
            }
            out << " ";
        } else if (fmt.kind == FormatKind::HEX) {
            out << " ";
            if (fmt.size == 1) {
                out << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(data[i]);
            } else if (fmt.size == 2) {
                uint16_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setfill('0') << std::setw(4) << std::hex << v;
            } else if (fmt.size == 4) {
                uint32_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setfill('0') << std::setw(8) << std::hex << v;
            }
        } else if (fmt.kind == FormatKind::OCTAL) {
            out << " ";
            if (fmt.size == 1) {
                out << std::setfill('0') << std::setw(3) << std::oct << static_cast<int>(data[i]);
            } else if (fmt.size == 2) {
                uint16_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setfill('0') << std::setw(6) << std::oct << v;
            }
        } else if (fmt.kind == FormatKind::UNSIGNED_DEC) {
            out << " ";
            if (fmt.size == 2) {
                uint16_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setw(5) << std::dec << v;
            } else if (fmt.size == 4) {
                uint32_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setw(10) << std::dec << v;
            }
        } else if (fmt.kind == FormatKind::SIGNED_DEC) {
            out << " ";
            if (fmt.size == 2) {
                int16_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setw(6) << std::dec << v;
            } else if (fmt.size == 4) {
                int32_t v = 0;
                std::memcpy(&v, data + i, available);
                out << std::setw(11) << std::dec << v;
            }
        }
    }
    out.copyfmt(state);
}

OdEngine::OdEngine(OdOptions opts) : options(std::move(opts)) {}

int OdEngine::execute() {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    std::vector<uint8_t> allBytes;

    for (const auto& fname : options.filenames) {
        if (fname == "-") {
            char buf[4096];
            while (std::cin.read(buf, sizeof(buf)) || std::cin.gcount() > 0) {
                allBytes.insert(allBytes.end(), buf, buf + std::cin.gcount());
            }
        } else {
            std::ifstream f(fname, std::ios::binary);
            if (!f.is_open()) {
                std::cerr << "od: " << fname << ": No such file or directory\n";
                continue;
            }
            char buf[4096];
            while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
                allBytes.insert(allBytes.end(), buf, buf + f.gcount());
            }
        }
    }

    std::ostringstream captured;
    std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;

    uint64_t start = (std::min)(options.skipBytes, static_cast<uint64_t>(allBytes.size()));
    uint64_t end = (std::min)(start + options.limitBytes, static_cast<uint64_t>(allBytes.size()));

    uint64_t currentOffset = start;
    while (currentOffset < end) {
        size_t chunkSize = static_cast<size_t>((std::min)(static_cast<uint64_t>(options.bytesPerLine), end - currentOffset));

        for (size_t fIdx = 0; fIdx < options.formats.size(); ++fIdx) {
            if (fIdx == 0) {
                OdFormatter::printAddress(*outStream, currentOffset, options.addressRadix);
            } else if (options.addressRadix != 'n') {
                *outStream << std::string(8, ' ');
            }
            OdFormatter::formatChunk(*outStream, allBytes.data() + currentOffset, chunkSize, options.formats[fIdx]);
            *outStream << "\n";
        }
        currentOffset += chunkSize;
    }

    if (options.addressRadix != 'n') {
        OdFormatter::printAddress(*outStream, currentOffset, options.addressRadix);
        *outStream << "\n";
    }

    if (options.outputFormat || !options.pipeCommand.empty()) {
        OdReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
    }

    return 0;
}
