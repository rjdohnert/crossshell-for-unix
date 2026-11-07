#ifndef DU_SCANNER_HPP
#define DU_SCANNER_HPP

#include "du_models.hpp"

#include <functional>

namespace du {

class DiskUsageScanner {
public:
    using EntryCallback = std::function<void(const UsageEntry&)>;

    explicit DiskUsageScanner(Options options);
    ScanSummary Scan(const std::vector<std::filesystem::path>& paths,
                     const EntryCallback& onEntry);

private:
    Options options_;
};

} // namespace du

#endif
