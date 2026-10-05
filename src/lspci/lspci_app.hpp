#ifndef LSPCI_APP_HPP
#define LSPCI_APP_HPP

#include "options.hpp"

class LspciApp {
private:
    CommandLineOptions options;
public:
    explicit LspciApp(CommandLineOptions opts);
    int run();
};

#endif // LSPCI_APP_HPP
