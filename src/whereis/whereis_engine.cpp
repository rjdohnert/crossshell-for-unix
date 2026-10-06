#include "location_searcher.hpp"
#include "target_result.hpp"
#include "whereis_engine.hpp"
#include "whereis_options.hpp"
#include "whereis_reporter.hpp"

WhereisEngine::WhereisEngine(WhereisOptions opts) : options(std::move(opts)) {}

int WhereisEngine::execute() {
        std::vector<TargetResult> results;

        for (const auto& target : options.targets) {
            TargetResult r;
            r.target = target;

            if (options.config.searchBin) {
                r.bins = LocationSearcher::searchCategory(target, options.config.binDirs, options.config.binExts);
            }
            if (options.config.searchMan) {
                r.mans = LocationSearcher::searchCategory(target, options.config.manDirs, options.config.manExts);
            }
            if (options.config.searchSrc) {
                r.srcs = LocationSearcher::searchCategory(target, options.config.srcDirs, options.config.srcExts);
            }

            results.push_back(r);
        }

        return WhereisReporter::dispatch(results, options.outputFormat, options.pipeCommand);
    }
