#include "stream_manager.hpp"

void StreamManager::configurePipes() {
        #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        #endif
    }
