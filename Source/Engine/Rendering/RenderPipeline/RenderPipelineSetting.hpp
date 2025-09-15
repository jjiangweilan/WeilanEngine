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
    struct PostProcess : Serializable
    {
        bool colorGrading = true;

        void Serialize(Serializer* s) const override { SERIALIZE(s, colorGrading); }
        void Deserialize(Serializer* s) override { DESERIALIZE(s, colorGrading); }
    } postProcess;

    struct ShadowMap : Serializable
    {
        float constantBias = 0.01f;
        float normalBias = 0.01f;

        void Serialize(Serializer* s) const override
        {
            SERIALIZE(s, constantBias);
            SERIALIZE(s, normalBias);
        }
        void Deserialize(Serializer* s) override
        {
            DESERIALIZE(s, constantBias);
            DESERIALIZE(s, normalBias);
        }
    } shadowMap;

    struct ContactShadow : Serializable
    {
        bool enabled = true;
        float thickness = 0.005f;
        void Serialize(Serializer* s) const override
        {
            SERIALIZE(s, enabled);
            SERIALIZE(s, thickness);
        }
        void Deserialize(Serializer* s) override
        {
            DESERIALIZE(s, enabled);
            DESERIALIZE(s, thickness);
        }
    } contactShadow;

    struct DebugDraw : Serializable
    {
        bool drawMeshRendererAABB = false;

        void Serialize(Serializer* s) const override { SERIALIZE(s, drawMeshRendererAABB); }
        void Deserialize(Serializer* s) override { DESERIALIZE(s, drawMeshRendererAABB); }
    } debugDraw;

    struct SSAO : Serializable
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

        void Serialize(Serializer* s) const override
        {
            SERIALIZE(s, enabled);
            SERIALIZE(s, enableUpscaler);
            SERIALIZE(s, debug_showNormal);
            SERIALIZE(s, strength);
            SERIALIZE(s, scaling);
            SERIALIZE(s, falloff);
            SERIALIZE(s, bias);
            SERIALIZE(s, bilateralUpScaleKernelSize);
            SERIALIZE(s, bilateralUpScaleIntegerCoordSigma);
            SERIALIZE(s, bilateralUpScaleDepthDiffSigma);
        }

        void Deserialize(Serializer* s) override
        {
            DESERIALIZE(s, enabled);
            DESERIALIZE(s, enableUpscaler);
            DESERIALIZE(s, debug_showNormal);
            DESERIALIZE(s, strength);
            DESERIALIZE(s, scaling);
            DESERIALIZE(s, falloff);
            DESERIALIZE(s, bias);
            DESERIALIZE(s, bilateralUpScaleKernelSize);
            DESERIALIZE(s, bilateralUpScaleIntegerCoordSigma);
            DESERIALIZE(s, bilateralUpScaleDepthDiffSigma);
        }
    } ssao;

    bool fxaa = true;
    bool frustumCull = true;
    bool shadowFrustumCull = true;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering
