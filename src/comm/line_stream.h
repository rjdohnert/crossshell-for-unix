#ifndef COMM_LINE_STREAM_H
#define COMM_LINE_STREAM_H

#include "comm.h"

namespace CommUtil {

// Line Stream Abstraction (Supports Files, Stdin, and Custom Delimiters)
class ILineStream {
public:
    virtual ~ILineStream() = default;
    virtual bool open() = 0;
    virtual bool get_next_line(std::string& out_line) = 0;
    virtual const std::string& get_name() const = 0;
    virtual uint64_t get_line_number() const = 0;
    virtual bool is_eof() const = 0;
};

class LineStream : public ILineStream {
public:
    LineStream(std::string name, char line_delim);

    bool open() override;
    bool get_next_line(std::string& out_line) override;
    const std::string& get_name() const override { return name_; }
    uint64_t get_line_number() const override { return line_number_; }
    bool is_eof() const override { return eof_; }

private:
    std::string name_;
    char line_delim_;
    std::istream* stream_ = nullptr;
    std::unique_ptr<std::ifstream> file_stream_;
    uint64_t line_number_ = 0;
    bool is_stdin_ = false;
    bool eof_ = false;
};

} // namespace CommUtil

#endif // COMM_LINE_STREAM_H
