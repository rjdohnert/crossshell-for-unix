#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "lspci.hpp"
#include <string>
#include <vector>
#include <memory>

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const = 0;
};

class ClassicFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override;
};

class TableFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override;
};

class JsonFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override;
};

class CsvFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override;
};

class PipelineFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override;
};

class FormatterFactory {
public:
    static std::unique_ptr<IOutputFormatter> create(const std::string& format);
};

#endif // REPORTER_HPP
