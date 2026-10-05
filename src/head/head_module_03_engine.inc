// ============================================================================
// 3. STREAM PROCESSOR ENGINE
// ============================================================================

class HeadEngine {
private:
    HeadOptions options;

public:
    explicit HeadEngine(HeadOptions opts) : options(std::move(opts)) {}

    static void stream(std::istream& in, long long limit, bool useBytes, std::ostream& out) {
        if (useBytes) {
            if (limit <= 0) return;
            char buffer[4096];
            long long remaining = limit;
            while (remaining > 0 && in) {
                long long toRead = (remaining < 4096) ? remaining : 4096;
                in.read(buffer, toRead);
                std::streamsize bytesRead = in.gcount();
                if (bytesRead > 0) {
                    out.write(buffer, bytesRead);
                    remaining -= bytesRead;
                } else {
                    break;
                }
            }
        } else {
            if (limit <= 0) return;
            char ch;
            long long count = 0;
            while (count < limit && in.get(ch)) {
                out.put(ch);
                if (ch == '\n') {
                    count++;
                }
            }
        }
    }

    int execute() {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        bool showHeaders = false;
        if (options.verbose) {
            showHeaders = true;
        } else if (options.quiet) {
            showHeaders = false;
        } else {
            showHeaders = (options.files.size() > 1);
        }

        bool success = true;
        std::ostringstream captured;
        std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;
        bool firstOutput = true;

        for (const auto& file : options.files) {
            long long limit = options.useBytes ? options.byteCount : options.lineCount;

            if (file == "-") {
                if (!firstOutput && showHeaders) {
                    *outStream << "\n";
                }
                if (showHeaders) {
                    *outStream << "==> standard input <==\n";
                }
                stream(std::cin, limit, options.useBytes, *outStream);
                firstOutput = false;
            } else {
                std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
                if (!infile.is_open()) {
                    std::cerr << "head: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                if (!firstOutput && showHeaders) {
                    *outStream << "\n";
                }
                if (showHeaders) {
                    *outStream << "==> " << file << " <==\n";
                }
                stream(infile, limit, options.useBytes, *outStream);
                firstOutput = false;
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            HeadReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return success ? 0 : 1;
    }
};

