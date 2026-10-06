#include "output_formatter.hpp"
#include "pattern_expander.hpp"
#include "tr_engine.hpp"
#include "tr_options.hpp"

TrEngine::TrEngine(TrOptions opts) : options(std::move(opts)) {}

int TrEngine::execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        constexpr size_t BUFFER_SIZE = 65536;
        std::vector<uint8_t> inBuf(BUFFER_SIZE);
        std::vector<uint8_t> outBuf(BUFFER_SIZE);
        size_t outPos = 0;
        std::string structuredOutput;

        auto flushOut = [&]() {
            if (outPos > 0) {
                if (options.outputFormat == 0 && options.pipeCommand.empty()) {
                    std::fwrite(outBuf.data(), 1, outPos, stdout);
                } else {
                    structuredOutput.append(reinterpret_cast<const char*>(outBuf.data()), outPos);
                }
                outPos = 0;
            }
        };

        auto emitByte = [&](uint8_t b) {
            outBuf[outPos++] = b;
            if (outPos == BUFFER_SIZE) {
                flushOut();
            }
        };

        // MODE 1: DELETE ONLY (-d)
        if (options.deleteChars && !options.squeeze) {
            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            bool delSet[256] = { false };
            for (uint8_t b : v1) delSet[b] = true;

            if (options.complement) {
                for (int i = 0; i < 256; ++i) delSet[i] = !delSet[i];
            }

            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!delSet[ch]) {
                        emitByte(ch);
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 2: DELETE AND SQUEEZE (-d -s)
        if (options.deleteChars && options.squeeze) {
            if (options.args.size() < 2) {
                std::cerr << "tr: missing operand after '" << options.args[0] << "' for -ds\n";
                return 1;
            }

            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            std::vector<uint8_t> v2 = PatternExpander::expand(options.args[1]);

            bool delSet[256] = { false };
            for (uint8_t b : v1) delSet[b] = true;
            if (options.complement) {
                for (int i = 0; i < 256; ++i) delSet[i] = !delSet[i];
            }

            bool sqzSet[256] = { false };
            for (uint8_t b : v2) sqzSet[b] = true;

            int lastCh = -1;
            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!delSet[ch]) {
                        if (!sqzSet[ch] || ch != lastCh) {
                            emitByte(ch);
                            lastCh = ch;
                        }
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 3: SQUEEZE ONLY (-s)
        if (options.squeeze && options.args.size() == 1) {
            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            bool sqzSet[256] = { false };
            for (uint8_t b : v1) sqzSet[b] = true;
            if (options.complement) {
                for (int i = 0; i < 256; ++i) sqzSet[i] = !sqzSet[i];
            }

            int lastCh = -1;
            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!sqzSet[ch] || ch != lastCh) {
                        emitByte(ch);
                        lastCh = ch;
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 4: TRANSLATION (STRING1 -> STRING2)
        if (options.args.size() < 2) {
            std::cerr << "tr: missing operand after '" << options.args[0] << "'\n";
            return 1;
        }

        std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
        std::vector<uint8_t> v2 = PatternExpander::expand(options.args[1], v1.size());

        if (v2.empty()) {
            std::cerr << "tr: string2 cannot be empty\n";
            return 1;
        }

        uint8_t mapTable[256];
        for (int i = 0; i < 256; ++i) mapTable[i] = static_cast<uint8_t>(i);

        bool sqzSet[256] = { false };
        if (options.squeeze) {
            for (uint8_t b : v2) sqzSet[b] = true;
        }

        if (options.complement) {
            bool inV1[256] = { false };
            for (uint8_t b : v1) inV1[b] = true;

            size_t v2Idx = 0;
            for (int i = 0; i < 256; ++i) {
                if (!inV1[i]) {
                    uint8_t target = (v2Idx < v2.size()) ? v2[v2Idx] : v2.back();
                    mapTable[i] = target;
                    v2Idx++;
                }
            }
        } else {
            for (size_t i = 0; i < v1.size(); ++i) {
                uint8_t src = v1[i];
                uint8_t dst = (i < v2.size()) ? v2[i] : v2.back();
                mapTable[src] = dst;
            }
        }

        int lastCh = -1;
        size_t bytesRead = 0;
        while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
            for (size_t i = 0; i < bytesRead; ++i) {
                uint8_t ch = mapTable[inBuf[i]];
                if (!options.squeeze || !sqzSet[ch] || ch != lastCh) {
                    emitByte(ch);
                    lastCh = ch;
                }
            }
        }
        flushOut();
        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
