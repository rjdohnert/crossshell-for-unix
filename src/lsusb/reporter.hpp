#pragma once

#include "lsusb.hpp"
#include <string>
#include <vector>
#include <memory>

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual std::string render(const std::vector<USBDevice>& devices, bool verbose) const = 0;
};

class ClassicFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<USBDevice>& devices, bool verbose) const override;
};

class TableFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<USBDevice>& devices, bool verbose) const override;
};

class JsonFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<USBDevice>& devices, bool verbose) const override;
};

class CsvFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<USBDevice>& devices, bool verbose) const override;
};

class PipelineFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<USBDevice>& devices, bool verbose) const override;
};

class FormatterFactory {
public:
    static std::unique_ptr<IOutputFormatter> create(const std::string& format);
};
