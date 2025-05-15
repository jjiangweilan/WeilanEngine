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

    struct ScreenSpaceShadow : Serializable
    {
        bool enabled;
        void Serialize(Serializer* s) const override { SERIALIZE(s, enabled); }
        void Deserialize(Serializer* s) override { DESERIALIZE(s, enabled); }
    } screenSpaceShadow;

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

    struct DebugDraw : Serializable
    {
        bool drawMeshRendererAABB = false;

        void Serialize(Serializer* s) const override { SERIALIZE(s, drawMeshRendererAABB); }
        void Deserialize(Serializer* s) override { DESERIALIZE(s, drawMeshRendererAABB); }
    } debugDraw;

    struct SSAO : Serializable
    {
        bool enabled = true;
        float strength = 50.f;
        float scaling = 1.f;
        float falloff = 50.f;
        float bias = 0.f;

        void Serialize(Serializer* s) const override
        {
            SERIALIZE(s, enabled);
            SERIALIZE(s, strength);
            SERIALIZE(s, scaling);
            SERIALIZE(s, falloff);
            SERIALIZE(s, bias);
        }

        void Deserialize(Serializer* s) override
        {
            DESERIALIZE(s, enabled);
            DESERIALIZE(s, strength);
            DESERIALIZE(s, scaling);
            DESERIALIZE(s, falloff);
            DESERIALIZE(s, bias);
        }
    } ssao;

    bool fxaa = true;
    bool frustumCull = true;
    bool shadowFrustumCull = true;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering
