#ifndef CROSSFORGE_COMPILER_DETECTOR_H
#define CROSSFORGE_COMPILER_DETECTOR_H

#include "crossforge.h"

class CompilerDetector {
public:
    struct DetectedTool {
        std::string Name;
        std::string Path;
        bool Found;
    };

    static std::vector<DetectedTool> DetectAll();
    static std::vector<std::string> GetVisualStudioBuildToolDirs();

    // Locate a Visual Studio developer-environment activation script so builds can
    // set up INCLUDE/LIB/PATH before invoking cl.exe. Prefers VsDevCmd.bat, falls
    // back to vcvars64.bat. Returns "" if none is found.
    static std::string FindVsDevCmdScript();

private:
    static std::string FindInPath(const std::string& exeName);
    static std::string SearchVisualStudio();
};

#endif // CROSSFORGE_COMPILER_DETECTOR_H
