#include "machinfo_app.hpp"
#include "engine.hpp"
#include <fcntl.h>
#include <io.h>

int MachinfoApp::run(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    MachinfoOptions options;
    if (!MachinfoOptions::parse(argc, argv, options)) {
        return 1;
    }
    MachinfoEngine engine(std::move(options));
    return engine.execute();
}
