#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/DrawList.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include <memory>

namespace GPUParameter
{
#include "Engine/Shaders/Shadows.hlsl"
}

namespace Rendering
{
struct ShadowRendererSettigns
{};
class ShadowRenderer
{
public:
    void Init();
    void SetSettings(ShadowRendererSettigns settings);
    void Setup(Light& light, RenderingData& renderingData);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData);
    Gfx::Image* GetShadowMap() { return shadowMap.get(); }
    float4 GetShadowMapTexelSize() { return shadowMapTexelSize; }
    float4x4 GetWorldToShadowMatrix(Light& light, RenderingData& renderingData, float shadowDistance);

private:
    Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
    Gfx::ImageIdentifier shadowMapId;
    Gfx::ImageDescription shadowDescription;
    std::unique_ptr<Gfx::Image> shadowMap;
    ObjPtr<Shader> shadowMapShader;
    ObjPtr<Shader> shadowMapShaderSkinned;
    ObjPtr<Shader> shadowMapShaderGPUDriven;
    std::vector<std::unique_ptr<Gfx::Buffer>> cascadeBuffers;

    bool updateMainLightShadow = true;

    struct ShadowMapInfo
    {
        int cascadeCount = 0;
    } currentShadowMapInfo;

    const float shadowMapWidth = 4096.0f;
    const glm::float4 shadowMapTexelSize = {1 / shadowMapWidth, 1 / shadowMapWidth, shadowMapWidth, shadowMapWidth};

    void ResetShadowmap(float shadowMapSizeScale, int cascadeCount);
};
} // namespace Rendering
