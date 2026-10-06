#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "patch.hpp"
#include "options.hpp"

class PatchEngine {
public:
    static void ReverseFilePatch(FilePatch& patch);
    static bool ApplyPatchToFile(const FilePatch& patch, const PatchOptions& opts);
};

#endif // ENGINE_HPP
