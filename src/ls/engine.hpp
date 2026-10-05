#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ls.hpp"
#include "options.hpp"
#include <vector>

class ListingEngine {
private:
    ListingOptions options;

    int getTerminalWidth() const;
    void sortItems(std::vector<FileItem>& items) const;

public:
    explicit ListingEngine(ListingOptions opts);

    void listDirectory(const fs::path& dirPath, bool printHeader = false);
    void listSingleItem(const fs::path& p);
    void listStructuredTarget(const fs::path& p);
};

#endif // ENGINE_HPP
