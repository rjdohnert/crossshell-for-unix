#include "sha256_engine.hpp"
#include "sha256_digest.hpp"
#include "sha256_reporter.hpp"
#include "checksum_verifier.hpp"

std::string Sha256Engine::computeStream(std::istream& is) {
    SHA256DigestEngine sha;
    char buffer[65536];
    while (is.read(buffer, sizeof(buffer)) || is.gcount() > 0) {
        sha.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(is.gcount()));
    }
    return sha.finalize();
}

Sha256Engine::Sha256Engine(Sha256Options opts) : options(std::move(opts)) {}

std::string Sha256Engine::computeFile(const std::string& filepath, bool binaryMode, bool& error) {
    error = false;
    if (filepath == "-") {
        _setmode(_fileno(stdin), binaryMode ? _O_BINARY : _O_TEXT);
        return computeStream(std::cin);
    }

    std::ios_base::openmode mode = std::ios::in;
    if (binaryMode) mode |= std::ios::binary;

    std::ifstream file(filepath, mode);
    if (!file.is_open()) {
        error = true;
        return "";
    }

    return computeStream(file);
}

int Sha256Engine::executeCheck() {
    size_t readFailures = 0;
    size_t checksumMismatches = 0;
    size_t formatErrors = 0;
    size_t totalChecked = 0;

    for (const auto& file : options.files) {
        std::istream* inStream = &std::cin;
        std::ifstream infile;

        if (file != "-") {
            infile.open(file);
            if (!infile.is_open()) {
                if (!options.status) {
                    std::cerr << "sha256sum: " << file << ": No such file or directory\n";
                }
                readFailures++;
                continue;
            }
            inStream = &infile;
        }

        std::string line;
        size_t lineNum = 0;

        while (std::getline(*inStream, line)) {
            lineNum++;
            if (line.empty() || line[0] == '#') continue;

            std::string expectedHash, filename;
            bool lineIsBinary = options.binaryMode;

            if (!Sha256ChecksumVerifier::parseLine(line, expectedHash, lineIsBinary, filename)) {
                formatErrors++;
                if (options.warn && !options.status) {
                    std::cerr << "sha256sum: " << file << ": " << lineNum << ": improperly formatted SHA256 checksum line\n";
                }
                continue;
            }

            bool err = false;
            std::string actualHash = computeFile(filename, lineIsBinary, err);
            totalChecked++;

            if (err) {
                readFailures++;
                if (!options.status) {
                    std::cout << filename << ": FAILED open or read\n";
                }
            } else {
                std::string expLower = expectedHash;
                std::transform(expLower.begin(), expLower.end(), expLower.begin(), ::tolower);

                if (actualHash == expLower) {
                    if (!options.status && !options.quiet) {
                        std::cout << filename << ": OK\n";
                    }
                } else {
                    checksumMismatches++;
                    if (!options.status) {
                        std::cout << filename << ": FAILED\n";
                    }
                }
            }
        }
    }

    if (!options.status) {
        if (formatErrors > 0 && !options.warn) {
            std::cerr << "sha256sum: WARNING: " << formatErrors << " line(s) improperly formatted\n";
        }
        if (readFailures > 0) {
            std::cerr << "sha256sum: WARNING: " << readFailures << " listed file(s) could not be read\n";
        }
        if (checksumMismatches > 0) {
            std::cerr << "sha256sum: WARNING: " << checksumMismatches << " computed checksum(s) did NOT match\n";
        }
    }

    return (checksumMismatches > 0 || readFailures > 0 || totalChecked == 0) ? 1 : 0;
}

int Sha256Engine::executeCompute() {
    int exitCode = 0;
    std::vector<Sha256Result> results;

    for (const auto& file : options.files) {
        bool err = false;
        std::string hash = computeFile(file, options.binaryMode, err);

        if (err) {
            std::cerr << "sha256sum: " << file << ": No such file or directory\n";
            exitCode = 1;
            results.push_back({ file, "", options.binaryMode, true });
        } else {
            results.push_back({ file, hash, options.binaryMode, false });
        }
    }

    Sha256Reporter::dispatch(results, options.outputFormat, options.pipeCommand);
    return exitCode;
}

int Sha256Engine::execute() {
    if (options.doCheck) {
        return executeCheck();
    }
    return executeCompute();
}
