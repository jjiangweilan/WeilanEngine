#include "RenderPipelineSetting.hpp"
#include "Engine/Library/Serialization/Serializer.hpp"
#include "Engine/Library/TypeReflection.hpp"

namespace Rendering
{
DEFINE_ASSET(RenderPipelineSetting, "55542C94-5DC2-4A3C-B778-01B380872E4D", "renderPipeline")

TYPE_REFLECTION_MEMBER_VARIABLES(
    RenderPipelineSetting,
    TYPE_REFLECTION_MEM(RenderPipelineSetting, shadowMap),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, contactShadow),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, antiAliasing),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, taa),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, useSSIL),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, postProcess),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, ssao),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, frustumCull),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, shadowFrustumCull),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, debugDraw),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, ssil),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, rtgi),
    TYPE_REFLECTION_MEM(RenderPipelineSetting, gi)
);

void RenderPipelineSetting::Serialize(Serializer* ser) const
{
    Asset::Serialize(ser);
    uint32_t antiAliasingValue = static_cast<uint32_t>(antiAliasing);
    ser->Serialize("antiAliasing", antiAliasingValue);
    ser->Serialize("taa", taa);
    ser->Serialize("shadowMap", shadowMap);
    ser->Serialize("contactShadow", contactShadow);
    ser->Serialize("useSSIL", useSSIL);
    ser->Serialize("postProcess", postProcess);
    ser->Serialize("ssao", ssao);
    ser->Serialize("frustumCull", frustumCull);
    ser->Serialize("shadowFrustumCull", shadowFrustumCull);
    ser->Serialize("ssil", ssil);
    ser->Serialize("rtgi", rtgi);
    ser->Serialize("gi", gi);
    ser->Serialize("debugDraw", debugDraw);
}

void RenderPipelineSetting::Deserialize(Serializer* ser)
{
    Asset::Deserialize(ser);

    bool legacyFxaa = true;
    ser->Deserialize("fxaa", legacyFxaa);
    uint32_t antiAliasingValue = static_cast<uint32_t>(
        legacyFxaa ? AntiAliasingMode::FXAA : AntiAliasingMode::None
    );
    ser->Deserialize("antiAliasing", antiAliasingValue);
    switch (static_cast<AntiAliasingMode>(antiAliasingValue))
    {
        case AntiAliasingMode::None:
        case AntiAliasingMode::FXAA:
        case AntiAliasingMode::TAA: antiAliasing = static_cast<AntiAliasingMode>(antiAliasingValue); break;
        default: antiAliasing = AntiAliasingMode::FXAA; break;
    }

    ser->Deserialize("taa", taa);
    ser->Deserialize("shadowMap", shadowMap);
    ser->Deserialize("contactShadow", contactShadow);
    ser->Deserialize("useSSIL", useSSIL);
    ser->Deserialize("postProcess", postProcess);
    ser->Deserialize("ssao", ssao);
    ser->Deserialize("frustumCull", frustumCull);
    ser->Deserialize("shadowFrustumCull", shadowFrustumCull);
    ser->Deserialize("ssil", ssil);
    ser->Deserialize("rtgi", rtgi);
    ser->Deserialize("gi", gi);
    ser->Deserialize("debugDraw", debugDraw);
}
} // namespace Rendering
