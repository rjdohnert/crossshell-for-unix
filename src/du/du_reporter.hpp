#ifndef DU_REPORTER_HPP
#define DU_REPORTER_HPP

#include "du_models.hpp"

#include <iosfwd>

namespace du {

class DuReporter {
public:
    DuReporter(const Options& options, std::ostream& output);
    void Begin();
    void WriteEntry(const UsageEntry& entry);
    void Finish(const ScanSummary& summary);

private:
    const Options& options_;
    std::ostream& output_;
    std::size_t entryCount_ = 0;
};

} // namespace du

#endif
