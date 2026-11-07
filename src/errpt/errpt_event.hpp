#ifndef ERRPT_EVENT_HPP
#define ERRPT_EVENT_HPP

#include <cstdint>
#include <string>

struct EventEntry {
    std::string timestamp;
    std::string severity;
    std::uint32_t eventId = 0;
    std::string source;
    std::string summary;
};

#endif
