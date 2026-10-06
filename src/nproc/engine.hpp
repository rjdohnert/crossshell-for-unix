#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "nproc.hpp"
#include "options.hpp"

class ProcessorCountResolver {
public:
    static DWORD calculateCount(const NprocOptions& opts);
};

class NprocEngine {
private:
    NprocOptions options;

public:
    explicit NprocEngine(NprocOptions opts);
    int execute();
};

#endif // ENGINE_HPP
