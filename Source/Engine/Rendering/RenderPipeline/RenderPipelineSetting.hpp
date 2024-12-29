#include "Core/Asset.hpp"

#pragma once
namespace Rendering
{
class RenderPipelineSetting : public Asset
{
    DECLARE_ASSET();

public:
    float shadowConstantBias = 0.01f;
    float shadowNormalBias = 0.01f;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering
