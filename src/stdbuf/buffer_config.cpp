#include "buffer_config.hpp"

BufferConfig BufferConfig::parse(const std::wstring& str) {
        BufferConfig cfg;
        if (str == L"0") {
            cfg.mode = BufferMode::Unbuffered;
            cfg.size = 1;
        } else if (str == L"L" || str == L"l") {
            cfg.mode = BufferMode::LineBuffered;
            cfg.size = 4096;
        } else {
            cfg.mode = BufferMode::BlockBuffered;
            try {
                cfg.size = std::stoull(str);
            } catch (...) {
                cfg.size = 4096;
            }
        }
        return cfg;
    }
