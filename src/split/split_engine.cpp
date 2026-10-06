#include "split_engine.hpp"
#include "split_options.hpp"
#include "suffix_generator.hpp"

SplitEngine::SplitEngine(SplitOptions opts)
        : options(opts), reporter(opts.outputFormat, opts.pipeCommand) {}

int SplitEngine::execute() {
        _setmode(_fileno(stdin), _O_BINARY);

        std::istream* inputStream = &std::cin;
        std::ifstream fileIn;

        if (options.inputFile != "-") {
            fileIn.open(options.inputFile, std::ios::binary);
            if (!fileIn.is_open()) {
                std::cerr << "split: cannot open '" << options.inputFile << "' for reading\n";
                return 1;
            }
            inputStream = &fileIn;
        }

        uint64_t filesCreated = 0;

        if (options.mode == SplitMode::Lines) {
            filesCreated = splitByLines(*inputStream);
        } else if (options.mode == SplitMode::Bytes) {
            filesCreated = splitByBytes(*inputStream);
        } else if (options.mode == SplitMode::Chunks) {
            filesCreated = splitByChunks(*inputStream);
        }

        return reporter.finish(options.prefix, filesCreated);
    }

uint64_t SplitEngine::splitByLines(std::istream& in) {
        uint64_t fileIdx = 0;
        std::string line;
        std::ofstream currentOut;
        uint64_t linesInCurrent = 0;

        while (std::getline(in, line)) {
            if (!currentOut.is_open() || linesInCurrent >= options.lineCount) {
                if (currentOut.is_open()) currentOut.close();
                std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
                currentOut.open(fname, std::ios::binary);
                linesInCurrent = 0;
            }

            currentOut << line << "\n";
            linesInCurrent++;
        }

        if (currentOut.is_open()) currentOut.close();
        return fileIdx;
    }

uint64_t SplitEngine::splitByBytes(std::istream& in) {
        uint64_t fileIdx = 0;
        constexpr size_t BUF_SIZE = 65536;
        std::vector<char> buffer(BUF_SIZE);
        std::ofstream currentOut;
        uint64_t bytesInCurrent = 0;

        while (in) {
            uint64_t remaining = options.byteCount - bytesInCurrent;
            size_t toRead = static_cast<size_t>(std::min(static_cast<uint64_t>(BUF_SIZE), remaining));
            in.read(buffer.data(), toRead);
            size_t bytesRead = static_cast<size_t>(in.gcount());

            if (bytesRead == 0) break;

            if (!currentOut.is_open()) {
                std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
                currentOut.open(fname, std::ios::binary);
                bytesInCurrent = 0;
            }

            currentOut.write(buffer.data(), bytesRead);
            bytesInCurrent += bytesRead;

            if (bytesInCurrent >= options.byteCount) {
                currentOut.close();
                bytesInCurrent = 0;
            }
        }

        if (currentOut.is_open()) currentOut.close();
        return fileIdx;
    }

uint64_t SplitEngine::splitByChunks(std::istream& in) {
        if (options.chunkCount == 0) return 0;

        std::vector<char> fullData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t totalBytes = fullData.size();
        size_t chunkSize = (totalBytes + options.chunkCount - 1) / options.chunkCount;

        uint64_t fileIdx = 0;
        size_t offset = 0;

        while (offset < totalBytes) {
            size_t curChunk = std::min(chunkSize, totalBytes - offset);
            std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
            std::ofstream out(fname, std::ios::binary);
            out.write(fullData.data() + offset, curChunk);
            out.close();
            offset += curChunk;
        }

        return fileIdx;
    }
