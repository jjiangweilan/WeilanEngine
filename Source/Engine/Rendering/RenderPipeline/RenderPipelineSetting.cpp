#include "RenderPipelineSetting.hpp"
namespace Rendering
{
DEFINE_ASSET(RenderPipelineSetting, "55542C94-5DC2-4A3C-B778-01B380872E4D", "renderPipeline")

DEFINE_SERIALIZATION(
    RenderPipelineSetting,
    Asset,
    SER(shadowMap),
    SER(contactShadow),
    SER(fxaa),
    SER(postProcess),
    SER(ssao),
    SER(frustumCull),
    SER(shadowFrustumCull),
    SER(debugDraw)
)
} // namespace Rendering
