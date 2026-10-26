#include "pv_internal.h"

int main(int argc, char *argv[]) {
    /* Ensure binary mode stream transfers on Windows */
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    PvConfig *config = PvConfig_New();
    if (!config->parse_args(config, argc, argv)) {
        free(config);
        return 1;
    }

    PvDisplay *display = PvDisplay_New(config->flag_force);
    PvEngine *engine = PvEngine_New(config, display);

    int exit_code = engine->run(engine);

    free(engine);
    free(display);
    free(config);

    return exit_code;
}
