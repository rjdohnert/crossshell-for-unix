#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

#include "cat_engine.hpp"
#include "cat_options.hpp"

int main(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    CatOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }
    if (options.unbuffered) {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }

    CatEngine engine(options);
    return engine.execute();
}
