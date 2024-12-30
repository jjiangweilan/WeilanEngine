#include "RenderPipelineSetting.hpp"
namespace Rendering
{
DEFINE_ASSET(RenderPipelineSetting, "55542C94-5DC2-4A3C-B778-01B380872E4D", "renderPipeline")

void RenderPipelineSetting::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    SERIALIZE(s, shadowMap);
    SERIALIZE(s, screenSpaceShadow);
}

void RenderPipelineSetting::Deserialize(Serializer* s) {
    Asset::Deserialize(s);
    DESERIALIZE(s, shadowMap);
    DESERIALIZE(s, screenSpaceShadow);
}
} // namespace Rendering
