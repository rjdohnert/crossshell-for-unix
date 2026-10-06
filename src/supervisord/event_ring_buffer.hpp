#pragma once

#include "supervisord.hpp"

class EventRingBuffer {
    static inline std::mutex g_evtMtx;
    static inline std::deque<std::string> g_events;
    static inline const size_t g_capacity = 200;
public:
    static void Add(const std::string& line);

    static std::string Dump(size_t limit = 50);
};
