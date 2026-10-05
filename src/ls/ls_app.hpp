#ifndef LS_APP_HPP
#define LS_APP_HPP

#include "options.hpp"

class LsApp {
private:
    ListingOptions options;

public:
    explicit LsApp(ListingOptions opts);
    int run();
};

#endif // LS_APP_HPP
