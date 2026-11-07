/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_options.hpp"

#include "dskctl_targets.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool SetOutputFormat(OutputFormat requested, DskctlOptions& options,
                    bool& formatWasSet, std::string& error) {
    if (formatWasSet && options.outputFormat != requested) {
        error = "Choose only one of --json, --csv, or --table.";
        return false;
    }
    options.outputFormat = requested;
    formatWasSet = true;
    return true;
}

bool SelectTarget(std::string_view flag, DskctlOptions& options,
                  std::vector<std::string>& individuallySelected) {
    auto found = std::find_if(options.targets.begin(), options.targets.end(),
        [flag](const CleanupTarget& target) { return target.flag == flag; });
    if (found == options.targets.end()) return false;
    found->selected = true;
    individuallySelected.push_back(found->id);
    return true;
}

bool IsIndividuallySelected(const std::vector<std::string>& ids,
                            const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

} // namespace

ParseResult ParseCommandLine(int argc, char* argv[]) {
    ParseResult result;
    if (!BuildCleanupTargets(result.options.targets, result.error)) return result;

    bool targetSpecified = false;
    bool allTargets = false;
    bool formatWasSet = false;
    bool executeWasSet = false;
    bool dryRunWasSet = false;
    std::vector<std::string> individuallySelected;

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            result.error = "Received a null command-line argument.";
            return result;
        }
        std::string_view argument(argv[index]);
        if (argument == "-h" || argument == "--help" || argument == "help") {
            result.options.showHelp = true;
            result.success = true;
            return result;
        }
        if (argument == "--json") {
            if (!SetOutputFormat(OutputFormat::Json, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "--csv") {
            if (!SetOutputFormat(OutputFormat::Csv, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "--table") {
            if (!SetOutputFormat(OutputFormat::Table, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "-n" || argument == "--dry-run") {
            if (executeWasSet) {
                result.error = "--dry-run and --execute cannot be combined.";
                return result;
            }
            dryRunWasSet = true;
            result.options.dryRun = true;
        } else if (argument == "-x" || argument == "--execute") {
            if (dryRunWasSet) {
                result.error = "--dry-run and --execute cannot be combined.";
                return result;
            }
            executeWasSet = true;
            result.options.dryRun = false;
        } else if (argument == "-t" || argument == "--trash") {
            result.options.useRecycleBin = true;
        } else if (argument == "-v" || argument == "--verbose") {
            result.options.verbose = true;
        } else if (argument == "-f" || argument == "--force") {
            result.options.force = true;
        } else if (argument == "-a" || argument == "--all") {
            targetSpecified = true;
            allTargets = true;
        } else if (argument == "-s" || argument == "--safe") {
            targetSpecified = true;
            result.options.safeOnly = true;
        } else if (SelectTarget(argument, result.options, individuallySelected)) {
            targetSpecified = true;
        } else {
            result.error = "Unknown option or target: " + std::string(argument);
            return result;
        }
    }

    if (result.options.safeOnly) {
        for (const CleanupTarget& target : result.options.targets) {
            if (target.risk == RiskLevel::High &&
                IsIndividuallySelected(individuallySelected, target.id)) {
                result.error = target.flag +
                    " cannot be combined with --safe because it is a high-risk target.";
                return result;
            }
        }
    }

    if (allTargets) {
        for (CleanupTarget& target : result.options.targets) target.selected = true;
    }
    if (result.options.safeOnly) {
        for (CleanupTarget& target : result.options.targets) {
            target.selected = target.risk != RiskLevel::High;
        }
    } else if (!targetSpecified) {
        for (CleanupTarget& target : result.options.targets) {
            target.selected = target.risk == RiskLevel::Low;
        }
    }

    if (!result.options.dryRun && !result.options.force &&
        result.options.outputFormat != OutputFormat::Human) {
        result.error = "Structured output with --execute requires --force.";
        return result;
    }

    result.success = true;
    return result;
}
