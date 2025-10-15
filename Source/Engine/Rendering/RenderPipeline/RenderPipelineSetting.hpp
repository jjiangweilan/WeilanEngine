#include "Core/Asset.hpp"
#include "Libs/Math.hpp"
#include "Libs/Serialization/Serializable.hpp"

#pragma once
namespace Rendering
{
class RenderPipelineSetting : public Asset
{
    DECLARE_ASSET();

public:
    struct PostProcess
    {
        bool colorGrading = true;

        INLINE_DEFINE_SERIALIZABLE(
            SER(colorGrading)
        );
    } postProcess;

    struct ShadowMap
    {
        float constantBias = 0.01f;
        float normalBias = 0.01f;

        INLINE_DEFINE_SERIALIZABLE(
            SER(constantBias),
            SER(normalBias)
        );

    } shadowMap;

    struct ContactShadow
    {
        bool enabled = true;
        float thickness = 0.005f;

        INLINE_DEFINE_SERIALIZABLE(
            SER(enabled),
            SER(thickness)
        )
    } contactShadow;

    struct DebugDraw
    {
        bool drawMeshRendererAABB = false;

        INLINE_DEFINE_SERIALIZABLE(
            SER(drawMeshRendererAABB)
        );

    } debugDraw;

    struct SSAO
    {
        bool enabled = true;
        bool enableUpscaler = true;
        bool debug_showNormal = false;
        float strength = 1.0;
        float scaling = 3.0f;
        float falloff = 1.5f;
        float bias = 0.f;
        float bilateralUpScaleKernelSize = 0.6;
        float bilateralUpScaleIntegerCoordSigma = 0.6;
        float bilateralUpScaleDepthDiffSigma = 0.6;

        INLINE_DEFINE_SERIALIZABLE(
            SER(enabled),
            SER(enableUpscaler),
            SER(debug_showNormal),
            SER(strength),
            SER(scaling),
            SER(falloff),
            SER(bias),
            SER(bilateralUpScaleKernelSize),
            SER(bilateralUpScaleIntegerCoordSigma),
            SER(bilateralUpScaleDepthDiffSigma)
        )

    } ssao;

    bool fxaa = true;
    bool frustumCull = true;
    bool shadowFrustumCull = true;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering
