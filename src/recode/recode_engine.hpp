#pragma once

#include "recode_options.hpp"
#include "recode.hpp"
#include "transformer_pipeline.hpp"

class RecodeEngine {
private:
    RecodeOptions options;

public:
    explicit RecodeEngine(RecodeOptions opts);

    int run();

private:
    bool processFile(const fs::path& path, const TransformerPipeline& pipeline);
};
