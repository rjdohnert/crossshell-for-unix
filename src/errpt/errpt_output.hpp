#ifndef ERRPT_OUTPUT_HPP
#define ERRPT_OUTPUT_HPP

#include "errpt_event.hpp"
#include "errpt_options.hpp"

#include <functional>
#include <string>
#include <string_view>

using OutputSink =
    std::function<bool(std::string_view, std::string&)>;

class EventOutput {
public:
    EventOutput(OutputFormat format, OutputSink sink);

    bool Begin(std::string& error);
    bool Write(const EventEntry& entry, std::string& error);
    bool Finish(std::string& error);

private:
    bool Emit(std::string_view value, std::string& error);

    OutputFormat format_;
    OutputSink sink_;
    bool firstEntry_ = true;
    bool began_ = false;
    bool finished_ = false;
};

#endif
