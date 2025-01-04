#include "Core/Asset.hpp"

#pragma once
namespace Rendering
{
class RenderPipelineSetting : public Asset
{
    DECLARE_ASSET();

public:
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

        void Serialize(Serializer* s) const override
        {
            SERIALIZE(s, drawMeshRendererAABB);
        }

        void Deserialize(Serializer* s) override
        {
            DESERIALIZE(s, drawMeshRendererAABB);
        }
    } debugDraw;

    bool fxaa = true;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering
