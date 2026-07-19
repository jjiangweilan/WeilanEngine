#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
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
class PointLightShadowRenderer
{
public:
    void Init();
    void Setup(Light& light, RenderingData& renderingData);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData);

    Gfx::Image* GetShadowCubemap() { return shadowCubemap.get(); }
    Gfx::ImageView* GetShadowCubemapView() { return cubemapSamplingView; }
    float GetFarPlane() const { return currentFarPlane; }
    float GetDepthBias() const { return currentDepthBias; }
    glm::vec3 GetLightPosition() const { return currentLightPosition; }

private:
    static constexpr uint32_t shadowMapSize = 512;

    Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
    std::unique_ptr<Gfx::Image> shadowCubemap;
    Gfx::ImageView* cubemapSamplingView = nullptr;  // owned by shadowCubemap
    std::array<std::unique_ptr<Gfx::ImageView>, 6> faceViews;
    std::array<std::unique_ptr<Gfx::Buffer>, 6> faceBuffers;

    ObjPtr<Shader> shadowMapShader;
    ObjPtr<Shader> shadowMapShaderGPUDriven;
    ObjPtr<Shader> shadowMapShaderTerrain;
    ObjPtr<Shader> terrainShader;

    float currentFarPlane = 10.0f;
    float currentDepthBias = 0.005f;
    glm::vec3 currentLightPosition = {0, 0, 0};

    bool initialized = false;

    void CreateCubemapResources();
    glm::mat4 GetFaceViewProjection(int faceIndex, const glm::vec3& lightPos, float nearPlane, float farPlane);
};
} // namespace Rendering
