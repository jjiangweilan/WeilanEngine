#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/Serialization/Serializable.hpp"

#pragma once
namespace Rendering
{
class RenderPipelineSetting : public Asset
{
    DECLARE_ASSET();
    DECLARE_SERIALIZATION()

public:
    bool fxaa = true;
    bool frustumCull = true;
    bool shadowFrustumCull = true;

    struct PostProcess
    {
        bool colorGrading = true;
        struct Bloom
        {
            bool enabled = true;
            float threshold = 1.0f;
            float intensity = 1.0f;
            float knee = 0.1f;
            float scatter = 0.7f;

            INLINE_DEFINE_SERIALIZABLE(
                SER(enabled),
                SER(threshold),
                SER(intensity),
                SER(knee),
                SER(scatter)
            )
        } bloom;

        INLINE_DEFINE_SERIALIZABLE(
            SER(colorGrading),
            SER(bloom)
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
        bool wireframe = false;
        bool motionVectors = false;
        bool hierarchyZBuffer = false;

        INLINE_DEFINE_SERIALIZABLE(
            SER(drawMeshRendererAABB),
            SER(wireframe),
            SER(motionVectors),
            SER(hierarchyZBuffer)
        );

    } debugDraw;

    struct SSAO
    {
        bool enabled = true;
        bool enableUpscaler = true;
        bool debug_showNormal = false;
        bool debug_ssaoOutput = false;
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
            SER(debug_ssaoOutput),
            SER(strength),
            SER(scaling),
            SER(falloff),
            SER(bias),
            SER(bilateralUpScaleKernelSize),
            SER(bilateralUpScaleIntegerCoordSigma),
            SER(bilateralUpScaleDepthDiffSigma)
        )

    } ssao;

    struct SSIL
    {
        bool enabled = false;
        float strength = 1.0f;
        float thickness = 0.1f;
        float radius = 1.0f;
        int sliceCount = 8;
        int sampleCount = 4;
        bool debug_ssilOutput = false;
        float jitterScale = 1.0f;
        float2 debugPoint;
        float filter1DepthDiffSigma = 1.0f;
        float filter2DepthDiffSigma = 1.0f;

        INLINE_DEFINE_SERIALIZABLE(
            SER(enabled),
            SER(strength),
            SER(thickness),
            SER(radius),
            SER(sliceCount),
            SER(sampleCount),
            SER(debug_ssilOutput),
            SER(jitterScale),
            SER(debugPoint),
            SER(filter1DepthDiffSigma),
            SER(filter2DepthDiffSigma)
        )
    } ssil;
};
} // namespace Rendering
