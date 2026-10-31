#include "common.h"
#include "cli.h"
#include "app.h"

int wmain(int argc, wchar_t* argv[]) {
    CliOptions* options = CliOptions_Create(argc, argv);
    if (!options) return 1;

    UncompressApp* app = UncompressApp_Create(options);
    if (!app) {
        CliOptions_Destroy(options);
        return 1;
    }

    int exitCode = UncompressApp_Run(app);

    UncompressApp_Destroy(app);
    CliOptions_Destroy(options);
    return exitCode;
}
