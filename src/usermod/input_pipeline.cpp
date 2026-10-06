#include "input_pipeline.hpp"
#include "mod_options.hpp"
#include "string_utils.hpp"
#include "windows_account_modifier.hpp"

bool InputPipeline::IsPipeActive() {
        return !_isatty(_fileno(stdin));
    }

int InputPipeline::ProcessBatchPipe(const ModOptions& baseOpt) {
        std::string line;
        size_t total = 0;
        size_t successful = 0;

        // Input format: username:key=value,key=value
        while (std::getline(std::cin, line)) {
            line = StringUtils::Trim(line);
            if (line.empty() || line[0] == '#') continue;

            auto colonIdx = line.find(':');
            if (colonIdx == std::string::npos) continue;

            std::string user = StringUtils::Trim(line.substr(0, colonIdx));
            std::string payload = line.substr(colonIdx + 1);

            ModOptions opt = baseOpt;
            opt.username = user;

            auto pairs = StringUtils::Split(payload, ',');
            for (const auto& pair : pairs) {
                auto eqIdx = pair.find('=');
                if (eqIdx == std::string::npos) continue;
                std::string k = StringUtils::Trim(pair.substr(0, eqIdx));
                std::string v = StringUtils::Trim(pair.substr(eqIdx + 1));

                if (k == "comment") opt.comment = v;
                else if (k == "fullname") opt.fullName = v;
                else if (k == "homedir") opt.homeDir = v;
                else if (k == "pass") opt.password = v;
                else if (k == "groups") opt.supplementaryGroups = StringUtils::Split(v, ';');
                else if (k == "lock") opt.lockAccount = (v == "true" || v == "1");
                else if (k == "login") opt.newLogin = v;
            }

            total++;
            std::string err;
            if (WindowsAccountModifier::ModifyAccount(opt, err)) {
                successful++;
                if (baseOpt.verbose) {
                    std::cout << "[SUCCESS] Modified account: " << user << "\n";
                }
            } else {
                std::cerr << "[ERROR] Failed updating user '" << user << "': " << err << "\n";
            }
        }

        std::cout << "Batch modification finished: " << successful << "/" << total << " accounts updated.\n";
        return (successful == total) ? 0 : 1;
    }
