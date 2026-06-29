#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Runtime/System/Rendering/DrawList.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
class Camera;
class Terrain;
class Light;
class Scene;
namespace Rendering
{
struct PerScene;
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
    glm::vec4 skyColor;
    glm::vec4 skyHorizonFalloffColor;
    glm::vec4 skyHorizonColor;
    glm::vec4 skySunColor;
    glm::vec4 skySunCoreColor;
};

struct InterleavedGradientNoise
{
    Gfx::Image* GetNoiseTexture() const;

private:
    mutable std::unique_ptr<Gfx::Image> interleavedGradientNoise;
    mutable Material interleavedGradientNoiseMat;
};

struct BlueNoise
{
    Gfx::Image* GetNoiseTexture() const;

private:
    mutable std::unique_ptr<Gfx::Image> blueNoise;
    mutable Material blueNoiseMat;
};

// GPU-Driven indirect draw
struct GPUObjectShaderGroup
{
    Gfx::ShaderProgram* shaderProgram = nullptr;
    const Gfx::PipelineConfig* pipelineConfig = nullptr;
    uint32_t firstDrawIndex = 0; // offset into indirectCommands
    uint32_t firstDynamicMotionDataIndex = 0;
    uint32_t drawCount = 0;
    bool hasMotion = false;
};

struct GPUDynamicMotionData
{
    glm::mat4 previousWorldMatrix = glm::mat4(1.0f);
    uint32_t previousSkeletonOffset = 0xFFFFFFFF;
    uint32_t padding0 = 0;
    uint32_t padding1 = 0;
    uint32_t padding2 = 0;
};

struct RenderingData
{
    PipelineGPUBufferAllocator* pipelineAllocator;
    Scene* scene;
    Camera* mainCamera;
    RenderPipelineSetting* renderPipelineSettings;
    RenderPipelineDebugger* renderPipelineDebugger;
    Frustum cameraFrustum;
    GPUParameter::Camera* gpuCamera;
    GPUParameter::Scene* gpuScene;
    GPUParameter::MainLightShadow* gpuMainLightShadow;
    Gfx::Image* mainColor;
    Gfx::Image* mainDepth;
    Gfx::Image* depthCopy;
    Gfx::Image* colorCopy;
    Gfx::Image* specularCubemap;
    std::vector<Light*> lights{};
    int mainLightIndex;
    int pointLightShadowIndex = -1;
    float2 screenSize;
    float screenAspect;
    InterleavedGradientNoise interleavedGradientNoise;
    BlueNoise blueNoise;
    PerScene* perScene;
    Gfx::ShaderResource* globalResource;

    std::vector<GPUObjectShaderGroup>* gpuObjectShaderGroups;
    Gfx::Buffer* gpuDrivenIndirectBuffer = nullptr;
    uint32_t gpuDrivenIndirectDrawCount = 0;

    Light* GetMainLight()
    {
        if (mainLightIndex >= 0 && mainLightIndex < lights.size())
            return lights[mainLightIndex];

        return nullptr;
    }
};
} // namespace Rendering
