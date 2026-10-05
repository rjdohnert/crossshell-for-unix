#include "lex_app.hpp"
#include "engine.hpp"
#include <iostream>
#include <fstream>

LexApp::LexApp(FlexOptions options)
    : m_opts(std::move(options)) {}

int LexApp::run() {
    if (m_opts.showHelp) {
        CommandLineParser::printHelp();
        return 0;
    }

    if (m_opts.showVersion) {
        CommandLineParser::printVersion();
        return 0;
    }

    // Determine input stream: File or Piped Stdin
    std::ifstream fileIn;
    std::istream* inStream = &std::cin;

    if (!m_opts.inputFilePath.empty() && m_opts.inputFilePath != "-") {
        fileIn.open(m_opts.inputFilePath);
        if (!fileIn.is_open()) {
            std::cerr << "lex: fatal error: cannot open input file '" << m_opts.inputFilePath << "'\n";
            return 1;
        }
        inStream = &fileIn;
    } else {
        if (!PipelineManager::isInputPiped() && m_opts.inputFilePath.empty()) {
            std::cerr << "lex: fatal error: no input files specified.\n";
            std::cerr << "Try 'lex --help' or 'lex -?' for more information.\n";
            return 1;
        }
    }

    if (m_opts.verbose) {
        std::cerr << "lex: parsing lexical specification...\n";
    }

    auto specData = SpecificationParser::parse(*inStream, m_opts);

    if (m_opts.verbose) {
        std::cerr << "lex: generating scanner code (Rules: " << specData.rules.size() << ")...\n";
    }

    std::string scannerCode = CodeGenerator::generateScanner(specData, m_opts);

    // Generate Header File if requested
    if (!m_opts.headerFile.empty()) {
        std::ofstream hOut(m_opts.headerFile);
        if (hOut.is_open()) {
            hOut << CodeGenerator::generateHeader(m_opts);
            if (m_opts.verbose) std::cerr << "lex: created header '" << m_opts.headerFile << "'\n";
        }
    }

    // Emit Scanner: To stdout (pipe) or output file
    if (m_opts.stdoutMode) {
        std::cout << scannerCode;
        std::cout.flush();
    } else {
        std::ofstream fOut(m_opts.outputFile);
        if (!fOut.is_open()) {
            std::cerr << "lex: error opening output file '" << m_opts.outputFile << "'\n";
            return 1;
        }
        fOut << scannerCode;
        if (m_opts.verbose) {
            std::cerr << "lex: scanner generated successfully: '" << m_opts.outputFile << "'\n";
        }
    }

    return 0;
}
