#include "yacc_app.hpp"
#include "yacc_cli.hpp"

int BisonApplication::run(int argc, char* argv[]) const { return bison_main(argc, argv); }
