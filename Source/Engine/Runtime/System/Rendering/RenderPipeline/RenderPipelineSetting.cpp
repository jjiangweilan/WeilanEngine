#include "RenderPipelineSetting.hpp"
#include "Engine/Library/TypeReflection.hpp"

namespace Rendering
{
DEFINE_ASSET(RenderPipelineSetting, "55542C94-5DC2-4A3C-B778-01B380872E4D", "renderPipeline")

TYPE_REFLECTION_MEMBER_VARIABLES(
    RenderPipelineSetting,
    TYPE_REFLECTION_MEM(RenderPipelineSetting, shadowMap),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, contactShadow),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, fxaa),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, postProcess),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, ssao),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, frustumCull),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, shadowFrustumCull),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, debugDraw),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, ssil),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, rtgi),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, gi)
);

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
    SER(ssil),
    SER(rtgi),
    SER(gi),
    SER(debugDraw)
)
} // namespace Rendering
