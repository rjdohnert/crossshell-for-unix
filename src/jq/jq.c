#include "jq_internal.h"

int main(int argc, char *argv[]) {
    /* Set Windows binary console streaming mode */
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    JqConfig *config = JqConfig_New();
    if (!config->parse_args(config, argc, argv)) {
        free(config);
        return 2;
    }

    JqEngine *engine = JqEngine_New(config);
    int status = engine->run(engine);
    JqEngine_Destroy(engine);

    return status;
}
