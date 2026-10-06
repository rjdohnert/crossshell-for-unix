#pragma once

#include "sync.hpp"

class ISyncTarget {
public:
    virtual ~ISyncTarget() = default;
    virtual bool flush(std::string& error_msg) = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
};

// Target representing a physical/logical Windows Volume (e.g., \\.\C:)
