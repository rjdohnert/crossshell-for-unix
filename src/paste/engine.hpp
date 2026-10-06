#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "paste.hpp"
#include "options.hpp"

class PasteEngine {
private:
    std::vector<std::string> m_delimiters;

public:
    explicit PasteEngine(std::vector<std::string> delimiters);
    void ExecuteSerial(const std::vector<std::string>& files) const;
    void ExecuteParallel(const std::vector<std::string>& files) const;
};

#endif // ENGINE_HPP
