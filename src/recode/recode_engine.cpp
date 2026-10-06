#include "pipe_stream_handler.hpp"
#include "recode_engine.hpp"
#include "recode_options.hpp"
#include "request_parser.hpp"
#include "transformer_pipeline.hpp"

RecodeEngine::RecodeEngine(RecodeOptions opts) : options(std::move(opts)) {}

int RecodeEngine::run() {
        if (options.listCharsets) {
            RecodeOptions::listSupported();
            return 0;
        }

        if (options.request.empty()) {
            std::cerr << "recode: missing conversion request.\n";
            std::cerr << "Try 'recode --help' for more information.\n";
            return 1;
        }

        auto pipeline = RequestParser::build(options.request);
        if (!pipeline || pipeline->empty()) {
            std::cerr << "recode: invalid request sequence '" << options.request << "'\n";
            return 1;
        }

        if (options.verbose) {
            std::cerr << "recode: active transformation pipeline [" << pipeline->getName() << "]\n";
        }

        // Pipe Mode: STDIN -> STDOUT
        if (options.fileList.empty() || (options.fileList.size() == 1 && options.fileList[0] == "-")) {
            auto inputData = PipeStreamHandler::readFromStdin();
            auto result = pipeline->transform(inputData);
            PipeStreamHandler::writeToStdout(result);
            return 0;
        }

        // In-Place File Mode
        int exitCode = 0;
        for (const auto& filePath : options.fileList) {
            if (!processFile(filePath, *pipeline)) {
                exitCode = 1;
            }
        }

        return exitCode;
    }

bool RecodeEngine::processFile(const fs::path& path, const TransformerPipeline& pipeline) {
        if (!fs::exists(path)) {
            std::cerr << "recode: " << path << ": No such file or directory\n";
            return false;
        }

        std::ifstream inFile(path, std::ios::binary);
        if (!inFile) {
            std::cerr << "recode: " << path << ": Permission denied\n";
            return false;
        }

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        auto output = pipeline.transform(buffer);

        // Write atomically using temporary file
        fs::path tempPath = path.string() + ".recode_tmp";
        std::ofstream outFile(tempPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "recode: failed to create temporary file for " << path << "\n";
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(output.data()), output.size());
        outFile.close();

        std::error_code ec;
        fs::rename(tempPath, path, ec);
        if (ec) {
            // Overwrite fallback if atomic swap is locked
            fs::copy_file(tempPath, path, fs::copy_options::overwrite_existing, ec);
            fs::remove(tempPath, ec);
        }

        if (options.verbose) {
            std::cerr << "recode: " << path << " successfully converted.\n";
        }

        return true;
    }
