class EntabEngine {
private:
    EntabOptions options;

    size_t getNextTabStop(size_t curCol) const {
        if (options.tabStops.empty()) {
            size_t w = options.singleTabWidth;
            if (w == 0) w = 8;
            return ((curCol / w) + 1) * w;
        } else {
            for (size_t stop : options.tabStops) {
                if (stop > curCol) {
                    return stop;
                }
            }
            return SIZE_MAX;
        }
    }

    bool flushPendingBlanks(size_t startCol, size_t endCol, bool hadTab, std::string* capture) const {
        size_t cur = startCol;

        auto writeChar = [capture](int value) {
            return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
        };

        while (cur < endCol) {
            size_t nextCol = getNextTabStop(cur);

            if (nextCol <= endCol) {
                size_t width = nextCol - cur;
                if (width > 1 || hadTab) {
                    if (writeChar('\t') == EOF) return false;
                    cur = nextCol;
                } else {
                    if (writeChar(' ') == EOF) return false;
                    cur++;
                }
            } else {
                size_t spacesToEmit = endCol - cur;
                for (size_t i = 0; i < spacesToEmit; ++i) {
                    if (writeChar(' ') == EOF) return false;
                }
                cur = endCol;
            }
        }
        return true;
    }

    bool processStream(FILE* fp, std::string* capture) const {
        setvbuf(fp, NULL, _IOFBF, EntabOptions::IO_BUFFER_SIZE);

        size_t col = 0;
        bool inInitialBlanks = true;
        bool inPendingRun = false;
        size_t pendingStartCol = 0;
        bool pendingHadTab = false;

        auto writeChar = [capture](int value) {
            return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
        };

        int ch = 0;
        while ((ch = fgetc(fp)) != EOF) {
            if (ch == ' ' || ch == '\t') {
                bool convert = options.convertAll || inInitialBlanks;

                if (convert) {
                    if (!inPendingRun) {
                        inPendingRun = true;
                        pendingStartCol = col;
                        pendingHadTab = false;
                    }
                    if (ch == '\t') {
                        pendingHadTab = true;
                        size_t nextStop = getNextTabStop(col);
                        col = (nextStop != SIZE_MAX) ? nextStop : (col + 1);
                    } else {
                        col++;
                    }
                } else {
                    if (inPendingRun) {
                        if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
                        inPendingRun = false;
                    }
                    if (writeChar(ch) == EOF) return false;
                    col++;
                }
            } else {
                if (inPendingRun) {
                    if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
                    inPendingRun = false;
                }

                if (writeChar(ch) == EOF) return false;

                if (ch == '\n') {
                    col = 0;
                    inInitialBlanks = true;
                } else if (ch == '\r') {
                    col = 0;
                } else if (ch == '\b') {
                    if (col > 0) col--;
                } else {
                    col++;
                    inInitialBlanks = false;
                }
            }
        }

        if (inPendingRun) {
            if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
        }

        return true;
    }

public:
    explicit EntabEngine(EntabOptions opts) : options(std::move(opts)) {}

    int execute(const char* progName) {
        if (options.showHelp) {
            EntabOptions::printHelp(progName);
            return 0;
        }

        if (options.showVersion) {
            EntabOptions::printVersion();
            return 0;
        }

        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        setvbuf(stdout, NULL, _IOFBF, EntabOptions::IO_BUFFER_SIZE);

        if (options.files.empty()) {
            options.files.push_back("-");
        }

        bool success = true;
        std::string captured;
        std::string* capturePtr = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : nullptr;

        for (const std::string& filename : options.files) {
            FILE* fp = nullptr;

            if (filename == "-") {
                fp = stdin;
            } else {
                errno_t err = fopen_s(&fp, filename.c_str(), "rb");
                if (err != 0 || !fp) {
                    std::cerr << "unexpand: " << filename << ": No such file or directory\n";
                    success = false;
                    continue;
                }
            }

            if (!processStream(fp, capturePtr)) {
                if (fp != stdin) fclose(fp);
                return 0;
            }

            if (fp != stdin) {
                fclose(fp);
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            EntabReporter::dispatch(captured, options.outputFormat, options.pipeCommand);
        }
        fflush(stdout);
        return success ? 0 : 1;
    }
};
