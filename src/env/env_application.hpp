#ifndef ENV_APPLICATION_HPP
#define ENV_APPLICATION_HPP

#include "env_options.hpp"

class EnvApplication {
public:
    explicit EnvApplication(ProgramOptions options);
    int Run();

private:
    ProgramOptions options_;
};

#endif
