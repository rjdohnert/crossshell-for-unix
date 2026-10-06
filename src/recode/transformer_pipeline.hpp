#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class TransformerPipeline : public ITransformer {
private:
    std::vector<std::unique_ptr<ITransformer>> stages;

public:
    void addStage(std::unique_ptr<ITransformer> stage);

    [[nodiscard]] bool empty() const;

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};
