#include "tee_app.hpp"
#include "tee_engine.hpp"
#include "tee_options.hpp"

int TeeApplication::Run(int argc, wchar_t* argv[]) const {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        TeeOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        return TeeEngine::Process(opts) ? 0 : 1;
    }
