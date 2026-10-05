#ifndef LOCATE_APP_HPP
#define LOCATE_APP_HPP

#include "options.hpp"

class LocateApp {
private:
    LocateOptions options;
public:
    explicit LocateApp(LocateOptions opts);
    int run();
};

#endif // LOCATE_APP_HPP
