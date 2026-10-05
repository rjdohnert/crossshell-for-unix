#include "reporter.hpp"
#include <iostream>
#include <iomanip>
#include <set>
#include <cctype>

ConsoleDeviceRenderer::ConsoleDeviceRenderer(const CommandLineOptions& options) : m_options(options) {}

void ConsoleDeviceRenderer::ShowHelp() const {
    std::cout << "Usage:\n"
              << "  lsdev -C [-c Class] [-s Subclass] [-t Type] [-l Name] [-S State] [-F Format] [-H]\n"
              << "  lsdev -P [-c Class] [-s Subclass] [-t Type] [-r Column] [-F Format] [-H]\n"
              << "  lsdev -r Column [-C | -P]\n"
              << "  lsdev [-h | --help]\n\n"
              << "Description:\n"
              << "  Displays device information and status for the host operating system.\n"
              << "\n"
              << "Qualifiers and Flags:\n"
              << "  -C           List Customized devices currently configured in the system (Default).\n"
              << "  -P           List Predefined device categories and system supported classes.\n"
              << "  -c Class     Displays devices belonging to the specified device class.\n"
              << "               (e.g., DiskDrive, Net, Display, Processor, System).\n"
              << "  -s Subclass  Displays devices connected via the specified subclass or bus\n"
              << "               (e.g., PCI, USB, ROOT, SCSI).\n"
              << "  -t Type      Displays devices matching the specified device type.\n"
              << "  -l Name      Displays information only for the device with the logical name.\n"
              << "               (e.g., hdisk0, ent0, proc0, sys0).\n"
              << "  -S State     Filters by device state (Available, Defined, Stopped).\n"
              << "  -H           Prints the column headers before displaying device records.\n"
              << "  -F Format    User-defined colon or custom token format. Supported tokens:\n"
              << "               name, status/state, location, description, class, subclass, type.\n"
              << "               Example: -F \"name:status:location:description\"\n"
              << "  -r Column    Displays all valid, distinct values for the specified column:\n"
              << "               (class, subclass, state, type).\n"
              << "  -h, --help   Displays this help manual.\n";
}

void ConsoleDeviceRenderer::Render(const std::vector<Device>& devices) const {
    if (!m_options.customFormat.empty()) {
        RenderFormatted(devices, m_options.customFormat);
        return;
    }
    if (!m_options.listColumn.empty()) {
        RenderColumnValues(devices, m_options.listColumn);
        return;
    }
    if (m_options.predefinedMode) {
        RenderPredefinedDevices(devices);
        return;
    }
    RenderDevices(devices);
}

void ConsoleDeviceRenderer::RenderDevices(const std::vector<Device>& devices) const {
    constexpr int nameWidth = 12;
    constexpr int statusWidth = 12;
    constexpr int locationWidth = 28;

    if (m_options.showHeaders) {
        std::cout << std::left 
                  << std::setw(nameWidth) << "NAME" << " | "
                  << std::setw(statusWidth) << "STATUS" << " | "
                  << std::setw(locationWidth) << "LOCATION" << " | "
                  << "DESCRIPTION\n"
                  << std::string(nameWidth, '-') << "-+-"
                  << std::string(statusWidth, '-') << "-+-"
                  << std::string(locationWidth, '-') << "-+-"
                  << std::string(32, '-') << "\n";
    }

    for (const auto& dev : devices) {
        std::cout << std::left 
                  << std::setw(nameWidth) << FitColumn(dev.GetName(), nameWidth) << " | "
                  << std::setw(statusWidth) << FitColumn(dev.GetState(), statusWidth) << " | "
                  << std::setw(locationWidth) << FitColumn(dev.GetLocation(), locationWidth) << " | "
                  << DisplayValue(dev.GetDescription()) << "\n";
    }
}

void ConsoleDeviceRenderer::RenderPredefinedDevices(const std::vector<Device>& devices) const {
    constexpr int classWidth = 18;
    constexpr int subclassWidth = 16;
    constexpr int typeWidth = 20;

    if (m_options.showHeaders) {
        std::cout << std::left 
                  << std::setw(classWidth) << "CLASS" << " | "
                  << std::setw(subclassWidth) << "SUBCLASS" << " | "
                  << std::setw(typeWidth) << "TYPE" << " | "
                  << "DESCRIPTION\n"
                  << std::string(classWidth, '-') << "-+-"
                  << std::string(subclassWidth, '-') << "-+-"
                  << std::string(typeWidth, '-') << "-+-"
                  << std::string(32, '-') << "\n";
    }

    std::set<std::string> seen;
    for (const auto& dev : devices) {
        std::string key = dev.GetClass() + "|" + dev.GetSubClass() + "|" + dev.GetType();
        if (seen.insert(key).second) {
            std::cout << std::left 
                      << std::setw(classWidth) << FitColumn(dev.GetClass(), classWidth) << " | "
                      << std::setw(subclassWidth) << FitColumn(dev.GetSubClass(), subclassWidth) << " | "
                      << std::setw(typeWidth) << FitColumn(dev.GetType(), typeWidth) << " | "
                      << DisplayValue(dev.GetDescription()) << "\n";
        }
    }
}

void ConsoleDeviceRenderer::RenderColumnValues(const std::vector<Device>& devices, const std::string& column) const {
    std::set<std::string> values;
    for (const auto& dev : devices) {
        std::string val = dev.GetField(column);
        if (!val.empty()) {
            values.insert(val);
        }
    }
    for (const auto& val : values) {
        std::cout << val << "\n";
    }
}

std::string ConsoleDeviceRenderer::DisplayValue(const std::string& value) {
    std::string display;
    display.reserve(value.size());
    bool pendingSpace = false;

    for (unsigned char ch : value) {
        if (std::isspace(ch)) {
            pendingSpace = !display.empty();
        } else {
            if (pendingSpace) display.push_back(' ');
            display.push_back(static_cast<char>(ch));
            pendingSpace = false;
        }
    }

    return display.empty() ? "-" : display;
}

std::string ConsoleDeviceRenderer::FitColumn(const std::string& value, size_t width) {
    const std::string display = DisplayValue(value);
    if (display.size() <= width) return display;
    if (width <= 3) return display.substr(0, width);
    return display.substr(0, width - 3) + "...";
}

void ConsoleDeviceRenderer::RenderFormatted(const std::vector<Device>& devices, const std::string& format) const {
    auto tokens = StringUtil::Split(format, ':');
    for (const auto& dev : devices) {
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::cout << dev.GetField(tokens[i]);
            if (i + 1 < tokens.size()) std::cout << ":";
        }
        std::cout << "\n";
    }
}
