#ifndef SHA256_ENGINE_HPP
#define SHA256_ENGINE_HPP

#include "sha256sum.hpp"
#include "sha256_options.hpp"

class Sha256Engine {
private:
    Sha256Options options;
    static std::string computeStream(std::istream& is);

public:
    explicit Sha256Engine(Sha256Options opts);
    static std::string computeFile(const std::string& filepath, bool binaryMode, bool& error);
    int executeCheck();
    int executeCompute();
    int execute();
};

#endif // SHA256_ENGINE_HPP
