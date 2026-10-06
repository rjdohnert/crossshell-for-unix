#pragma once

#include "recode.hpp"
#include "transformer_pipeline.hpp"
#include "transformer.hpp"

class RequestParser {
public:
    static std::optional<uint32_t> resolveCodePage(std::string_view name);

    static std::unique_ptr<ITransformer> createSurface(std::string_view name, bool encode);

    static std::unique_ptr<TransformerPipeline> build(const std::string& requestStr);
};
