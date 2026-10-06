#include "transformer_pipeline.hpp"
#include "transformer.hpp"

void TransformerPipeline::addStage(std::unique_ptr<ITransformer> stage) {
        if (stage) stages.push_back(std::move(stage));
    }

[[nodiscard]] bool TransformerPipeline::empty() const { return stages.empty(); }

[[nodiscard]] std::string TransformerPipeline::getName() const  {
        std::string desc;
        for (size_t i = 0; i < stages.size(); ++i) {
            desc += stages[i]->getName();
            if (i + 1 < stages.size()) desc += " -> ";
        }
        return desc;
    }

[[nodiscard]] std::vector<uint8_t> TransformerPipeline::transform(const std::vector<uint8_t>& input) const  {
        std::vector<uint8_t> current = input;
        for (const auto& stage : stages) {
            current = stage->transform(current);
        }
        return current;
    }
