/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dirtree_app.hpp"
#include "models.hpp"
#include "options.hpp"
#include "path_helper.hpp"
#include "traversal_engine.hpp"

#include <iostream>
#include <sstream>
#include <cstdio>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace dirtree {

int DirtreeApplication::Run(int argc, char* argv[]) const {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    ConsoleEnvironment::InitConsole();
    TreeOptions config;

    if (!config.Parse(argc, argv)) {
        return 1;
    }

    if (config.showHelp) {
        config.PrintHelp();
        return 0;
    }
    if (config.showVersion) {
        config.PrintVersion();
        return 0;
    }

    fs::path root(config.rootPath);
    if (!fs::exists(root)) {
        std::cerr << Color::RED << "Error: Target path '" << config.rootPath << "' does not exist." << Color::RESET << "\n";
        return 1;
    }

    if (!config.colorModeExplicit) {
        config.useColor = ConsoleEnvironment::ShouldUseColorByDefault();
    }

    std::ostringstream captured;
    std::ostream* outStream = (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) ? &captured : &std::cout;

    *outStream << (config.useColor ? Color::B_CYAN : "") << PathHelper::PathToUtf8(root) << (config.useColor ? Color::RESET : "") << "\n";

    TreeStats stats;
    TreeTraversalEngine::PrintTree(root, "", 0, config, stats, *outStream);

    *outStream << "\n";
    if (config.useColor) {
        *outStream << Color::BOLD << stats.dirCount << Color::RESET;
    } else {
        *outStream << stats.dirCount;
    }
    *outStream << " directories";
    if (!config.dirsOnly) {
        *outStream << ", ";
        if (config.useColor) {
            *outStream << Color::BOLD << stats.fileCount << Color::RESET;
        } else {
            *outStream << stats.fileCount;
        }
        *outStream << " files";
    }
    *outStream << "\n";

    if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
        std::string data = captured.str();
        std::string text = (config.outputFormat == OutputFormat::Json) ? "{\"root\":\"" + PathHelper::PathToUtf8(root) + "\",\"output\":\"" + data + "\"}\n" :
                           (config.outputFormat == OutputFormat::Csv) ? "root,output\n" + PathHelper::PathToUtf8(root) + ",\"" + data + "\"\n" :
                           "ROOT\tOUTPUT\n" + PathHelper::PathToUtf8(root) + "\t" + data;

        if (!config.pipeCommand.empty()) {
            FILE* pipe = POPEN(config.pipeCommand.c_str(), "w");
            if (pipe) {
                std::fwrite(text.data(), 1, text.size(), pipe);
                PCLOSE(pipe);
            }
        } else {
            std::cout << text;
        }
    }

    return 0;
}

} // namespace dirtree
