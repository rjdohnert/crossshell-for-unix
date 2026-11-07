#ifndef ENTAB_ENGINE_HPP
#define ENTAB_ENGINE_HPP

#include "entab_models.hpp"
#include "entab_reporter.hpp"

#include <istream>

class EntabEngine {
public:
    explicit EntabEngine(const EntabOptions& options);
    bool Convert(std::istream& input, EntabReporter& reporter,
                 std::string& error) const;

private:
    bool GetNextTabStop(std::size_t column, std::size_t& stop,
                        bool& hasStop) const;
    bool FlushPendingBlanks(std::size_t startColumn,
                            std::size_t endColumn, bool hadTab,
                            EntabReporter& reporter,
                            std::string& error) const;
    bool WriteCharacter(unsigned char value, EntabReporter& reporter,
                        std::string& error) const;

    EntabOptions options_;
};

#endif
