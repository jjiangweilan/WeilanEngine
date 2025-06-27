#pragma once
#include "Core/Math/Geometry.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
#include "Rendering/GPUParameter.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
namespace GPUParameter
{
#include "Shaders/DeferredPBRShadingInput.hlsl"
#include "Shaders/Library/PerScene.hlsl"
} // namespace GPUParameter
class Camera;
class Terrain;
class Light;
class Scene;
namespace Rendering
{
class RenderPipelineDebugger;
struct LightInfo
{
    glm::vec4 lightColor;
    glm::vec4 position;
    float ambientScale;
    float range;
    float intensity;
    float pointLightTerm1;
    float pointLightTerm2;

    float p0, p1, p2; // padding
};

struct InterleavedGradientNoise
{
    Gfx::Image* GetNoiseTexture() const;

private:
    mutable std::unique_ptr<Gfx::Image> interleavedGradientNoise;
    mutable Material interleavedGradientNoiseMat;
};

struct RenderingData
{
    Scene* scene;
    Camera* mainCamera;
    RenderPipelineSetting* renderPipelineSettings;
    RenderPipelineDebugger* renderPipelineDebugger;
    Frustum cameraFrustum;
    GPUParameter::PerScene* sceneInfo;
    GPUParameter::Camera* gpuCamera;
    GPUParameter::Scene* gpuScene;
    GPUParameter::MainLightShadow* gpuMainLightShadow;
    Gfx::Image* mainColor;
    Gfx::Image* mainDepth;
    Gfx::Image* depthCopy;
    DynamicArray<Light*> lights{};
    int mainLightIndex;
    float2 screenSize;
    float screenAspect;
    InterleavedGradientNoise interleavedGradientNoise;

    Light* GetMainLight()
    {
        if (mainLightIndex >= 0 && mainLightIndex < lights.size())
            return lights[mainLightIndex];

        return nullptr;
    }
};
} // namespace Rendering
