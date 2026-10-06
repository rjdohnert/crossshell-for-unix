#include "script.hpp"
#include "script_options.hpp"
#include "session_recorder.hpp"

class ScriptApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        ScriptOptions options;
        if (!ScriptOptions::parse(argc, argv, options)) {
            return 1;
        }

        ScriptEngine engine(std::move(options));
        return engine.execute(argv[0]);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return ScriptApp::run(argc, argv);
}
