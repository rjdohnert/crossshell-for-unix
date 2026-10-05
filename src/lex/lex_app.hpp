#ifndef LEX_APP_HPP
#define LEX_APP_HPP

#include "options.hpp"

class LexApp {
private:
    FlexOptions m_opts;
public:
    explicit LexApp(FlexOptions options);
    int run();
};

#endif // LEX_APP_HPP
