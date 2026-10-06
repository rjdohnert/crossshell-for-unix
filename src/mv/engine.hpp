#ifndef MV_ENGINE_HPP
#define MV_ENGINE_HPP

#include "mv.hpp"

class MoveEngine {
private:
    MoveOptions options;
    ProgressBar progressBar;

    bool promptOverwrite(const fs::path& dest);
    bool prepareDestination(const fs::path& dest, bool& replaceTarget);
    bool copyAndUnlinkFile(const fs::path& src, const fs::path& dest);
    bool moveDirectoryCrossVolume(const fs::path& src, const fs::path& dest);
    bool moveItem(const fs::path& src, const fs::path& dest);

public:
    explicit MoveEngine(const MoveOptions& opts);
    bool execute();
};

#endif // MV_ENGINE_HPP
