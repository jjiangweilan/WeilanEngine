#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Shader2.hpp"
#include <memory>

namespace Rendering
{
struct ShadowRendererSettigns
{};
class ShadowRenderer
{
public:
    void Init();
    void SetSettings(ShadowRendererSettigns settings);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, DrawList& sceneDrawList);
    Gfx::Image* GetShadowMap() { return shadowMap.get(); }
    float4 GetShadowMapTexelSize() { return shadowMapTexelSize; }

private:
    Gfx::RG::RenderPass pass = Gfx::RG::RenderPass(1, 1);
    Gfx::RG::ImageIdentifier shadowMapId;
    Gfx::ImageDescription shadowDescription;
    std::unique_ptr<Gfx::Image> shadowMap;
    ObjPtr<Shader2> shadowMapShader;
    ObjPtr<Shader2> shadowMapShaderSkinned;

    bool updateMainLightShadow = true;

    const float shadowMapWidth = 1024.0f;
    const glm::float4 shadowMapTexelSize = {1 / shadowMapWidth, 1 / shadowMapWidth, shadowMapWidth, shadowMapWidth};
};
} // namespace Rendering
