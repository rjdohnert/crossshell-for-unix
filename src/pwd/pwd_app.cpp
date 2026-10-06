#include "pwd_app.hpp"

int PwdApp::run(int argc, wchar_t* argv[]) {
    PwdOptions options;
    if (!PwdOptions::parse(argc, argv, options)) {
        return 1;
    }
    return PwdEngine::execute(options);
}
