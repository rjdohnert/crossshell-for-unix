#ifndef MKBOOT_ENGINE_HPP
#define MKBOOT_ENGINE_HPP

#include "mkboot.hpp"
#include "options.hpp"

class WinPEManager {
public:
    static bool CustomizeImage(const fs::path& wimPath, const fs::path& mountDir, const ConfigOptions& config);
};

class IsoBuilder {
public:
    static bool BuildIso(const fs::path& sourceTree, const fs::path& bootFileOverride, const fs::path& outputIso, std::string volumeLabel);
private:
    static bool WriteStreamToFile(IStream* pStream, const fs::path& outputPath);
};

#endif // MKBOOT_ENGINE_HPP
