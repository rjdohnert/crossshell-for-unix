#include "event_ring_buffer.hpp"

void EventRingBuffer::Add(const std::string& line) {
        std::lock_guard<std::mutex> lock(g_evtMtx);
        if (g_events.size() >= g_capacity) g_events.pop_front();
        g_events.push_back(line);
    }

std::string EventRingBuffer::Dump(size_t limit ) {
        std::lock_guard<std::mutex> lock(g_evtMtx);
        if (g_events.empty()) return "<no events>\n";
        size_t count = std::min(limit, g_events.size());
        size_t start = g_events.size() - count;
        std::stringstream ss;
        for (size_t i = start; i < g_events.size(); ++i) ss << g_events[i] << "\n";
        return ss.str();
    }
