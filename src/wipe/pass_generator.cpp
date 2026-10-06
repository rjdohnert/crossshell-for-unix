#include "pass_config.hpp"
#include "pass_generator.hpp"
#include "path_utils.hpp"

std::vector<PassConfig> PassGenerator::GetPassConfigs(int passes, bool dod_mode, bool quick_mode) {
        std::vector<PassConfig> configs;
        if (quick_mode) {
            configs.push_back({PASS_RANDOM, 0});
            configs.push_back({PASS_ZERO, 0x00});
            return configs;
        }
        if (dod_mode) {
            configs.push_back({PASS_ZERO, 0x00});
            configs.push_back({PASS_ONE, 0xFF});
            configs.push_back({PASS_RANDOM, 0});
            return configs;
        }

        if (passes <= 1) {
            configs.push_back({PASS_RANDOM, 0});
        } else if (passes == 2) {
            configs.push_back({PASS_RANDOM, 0});
            configs.push_back({PASS_ZERO, 0x00});
        } else {
            configs.push_back({PASS_ZERO, 0x00});
            configs.push_back({PASS_ONE, 0xFF});
            for (int i = 2; i < passes - 1; ++i) {
                if (i % 2 == 0) configs.push_back({PASS_PATTERN, 0xAA});
                else configs.push_back({PASS_PATTERN, 0x55});
            }
            configs.push_back({PASS_RANDOM, 0});
        }
        return configs;
    }

void PassGenerator::FillBuffer(std::vector<char>& buf, const PassConfig& cfg) {
        if (cfg.type == PASS_ZERO) {
            std::fill(buf.begin(), buf.end(), 0x00);
        } else if (cfg.type == PASS_ONE) {
            std::fill(buf.begin(), buf.end(), static_cast<char>(0xFF));
        } else if (cfg.type == PASS_PATTERN) {
            std::fill(buf.begin(), buf.end(), static_cast<char>(cfg.pattern));
        } else if (cfg.type == PASS_RANDOM) {
            std::uniform_int_distribution<unsigned int> dist(0, 255);
            for (size_t i = 0; i < buf.size(); ++i) {
                buf[i] = static_cast<char>(dist(PathUtils::Rng()));
            }
        }
    }
