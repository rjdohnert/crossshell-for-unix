#include "path_candidate_resolver.hpp"
#include "which_engine.hpp"
#include "which_options.hpp"
#include "which_reporter.hpp"

WhichEngine::WhichEngine(WhichOptions opts) : options(std::move(opts)) {}

int WhichEngine::execute() {
        const char* pathEnv = std::getenv("PATH");
        std::vector<std::string> pathDirs;
        if (pathEnv) {
            pathDirs = PathCandidateResolver::split(pathEnv, ';');
        }

        const char* pathextEnv = std::getenv("PATHEXT");
        std::vector<std::string> pathexts;
        if (pathextEnv) {
            pathexts = PathCandidateResolver::split(pathextEnv, ';');
            for (auto& ext : pathexts) {
                ext = PathCandidateResolver::toLower(ext);
            }
        } else {
            pathexts = { ".com", ".exe", ".bat", ".cmd" };
        }

        bool allFound = true;
        std::vector<std::pair<std::string, std::string>> allMatches;

        for (const auto& cmd : options.commands) {
            bool found = false;
            fs::path cmdPath(cmd);

            if (cmd.find('/') != std::string::npos || cmd.find('\\') != std::string::npos) {
                for (const auto& cand : PathCandidateResolver::getCandidates(cmdPath, pathexts)) {
                    if (PathCandidateResolver::isExecutable(cand)) {
                        found = true;
                        std::string full = fs::absolute(cand).lexically_normal().string();
                        if (!options.silent && options.outputFormat == 0 && options.pipeCommand.empty()) {
                            std::cout << full << "\n";
                        }
                        allMatches.emplace_back(cmd, full);
                        if (!options.all) break;
                    }
                }
            } else {
                for (auto dirStr : pathDirs) {
                    dirStr = PathCandidateResolver::trimQuotes(dirStr);
                    if (dirStr.empty()) continue;

                    fs::path dir(dirStr);
                    fs::path base = dir / cmdPath;

                    for (const auto& cand : PathCandidateResolver::getCandidates(base, pathexts)) {
                        if (PathCandidateResolver::isExecutable(cand)) {
                            found = true;
                            std::string full = fs::absolute(cand).lexically_normal().string();
                            if (!options.silent && options.outputFormat == 0 && options.pipeCommand.empty()) {
                                std::cout << full << "\n";
                            }
                            allMatches.emplace_back(cmd, full);
                            if (!options.all) break;
                        }
                    }

                    if (found && !options.all) break;
                }
            }

            if (!found) {
                allFound = false;
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            WhichReporter::dispatch(allMatches, options.outputFormat, options.pipeCommand);
        }

        return allFound ? 0 : 1;
    }
