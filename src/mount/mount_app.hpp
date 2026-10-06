#ifndef MOUNT_APP_HPP
#define MOUNT_APP_HPP

#include "mount.hpp"
#include "options.hpp"
#include "engine.hpp"

class MountApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // MOUNT_APP_HPP
