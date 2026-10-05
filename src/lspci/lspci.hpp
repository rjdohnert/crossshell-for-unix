#ifndef LSPCI_HPP
#define LSPCI_HPP

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#if __cplusplus >= 201703L
#include <optional>
#else
#include <utility>
namespace std {
template <typename T>
class optional {
public:
    optional() : hasValue_(false) {}
    optional(const T& value) : value_(value), hasValue_(true) {}
    optional(T&& value) : value_(std::move(value)), hasValue_(true) {}
    optional(const optional&) = default;
    optional(optional&&) noexcept = default;
    optional& operator=(const optional&) = default;
    optional& operator=(optional&&) noexcept = default;

    [[nodiscard]] bool has_value() const noexcept { return hasValue_; }
    explicit operator bool() const noexcept { return hasValue_; }

    [[nodiscard]] T& value() { return value_; }
    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] T& operator*() { return value_; }
    [[nodiscard]] const T& operator*() const { return value_; }
    [[nodiscard]] T* operator->() { return &value_; }
    [[nodiscard]] const T* operator->() const { return &value_; }

    [[nodiscard]] T value_or(const T& defaultValue) const {
        return hasValue_ ? value_ : defaultValue;
    }

    void reset() noexcept { hasValue_ = false; }

private:
    T value_{};
    bool hasValue_;
};
}
#endif

class PCIDevice {
public:
    int domain;
    int bus;
    int slot;
    int func;
    std::string vid;
    std::string did;
    std::string className;
    std::string vendorName;
    std::string deviceName;
    std::string hwPath;     // hardware path (e.g., 0/0/8/0.0)
    std::string driver;     // Active kernel service/driver module
    std::string subsystem;
    std::string revision;

    PCIDevice(int dom, int b, int s, int f,
              std::string v, std::string d,
              std::string cls, std::string vName, std::string dName,
              std::string path = "0/0/0/0", std::string drv = "N/A",
              std::string sub = "N/A", std::string rev = "00")
        : domain(dom), bus(b), slot(s), func(f),
          vid(std::move(v)), did(std::move(d)),
          className(std::move(cls)), vendorName(std::move(vName)),
          deviceName(std::move(dName)), hwPath(std::move(path)),
          driver(std::move(drv)), subsystem(std::move(sub)),
          revision(std::move(rev)) {}

    [[nodiscard]] std::string getDbdf() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(4) << domain << ":"
            << std::setw(2) << bus << ":"
            << std::setw(2) << slot << "."
            << func;
        return oss.str();
    }

    [[nodiscard]] std::string getShortSlot() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(2) << bus << ":"
            << std::setw(2) << slot << "."
            << func;
        return oss.str();
    }

    [[nodiscard]] std::string getId() const {
        return vid + ":" + did;
    }

    [[nodiscard]] bool matches(std::optional<int> filterDom,
                               std::optional<int> filterBus,
                               std::optional<int> filterSlot,
                               std::optional<int> filterFunc,
                               const std::string& filterVid,
                               const std::string& filterDid) const {
        if (filterDom.has_value() && domain != *filterDom) return false;
        if (filterBus.has_value() && bus != *filterBus) return false;
        if (filterSlot.has_value() && slot != *filterSlot) return false;
        if (filterFunc.has_value() && func != *filterFunc) return false;
        if (!filterVid.empty() && vid != filterVid) return false;
        if (!filterDid.empty() && did != filterDid) return false;
        return true;
    }
};

#endif // LSPCI_HPP
