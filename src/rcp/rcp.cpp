#include "rcp.hpp"
#include "rcp_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    RcpApplication app;
    return app.Run(argc, argv);
}
