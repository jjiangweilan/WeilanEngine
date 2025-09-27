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
    void Setup(RenderingData& renderingData);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData);
    Gfx::Image* GetShadowMap() { return shadowMap.get(); }
    float4 GetShadowMapTexelSize() { return shadowMapTexelSize; }
    float4x4 GetShadowToWorldMatrix(RenderingData& renderingData);

private:
    Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
    Gfx::ImageIdentifier shadowMapId;
    Gfx::ImageDescription shadowDescription;
    std::unique_ptr<Gfx::Image> shadowMap;
    ObjPtr<Shader2> shadowMapShader;
    ObjPtr<Shader2> shadowMapShaderSkinned;

    bool updateMainLightShadow = true;

    const float shadowMapWidth = 4096.0f;
    const glm::float4 shadowMapTexelSize = {1 / shadowMapWidth, 1 / shadowMapWidth, shadowMapWidth, shadowMapWidth};
};
} // namespace Rendering
