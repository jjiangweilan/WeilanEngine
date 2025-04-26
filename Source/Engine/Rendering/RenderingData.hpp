#pragma once
#include "Core/Math/Geometry.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <glm/glm.hpp>
namespace GPUParameter
{
using namespace glm;
#include "Shaders/DeferredPBRShadingInput.hlsl"
#include "Shaders/Library/PerScene.hlsl"
} // namespace GPUParameter
class Camera;
class Terrain;
class Light;
class Scene;
namespace Rendering
{
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
    Frustum cameraFrustum;
    GPUParameter::PerScene* sceneInfo;
    Gfx::Image* mainColor;
    Gfx::Image* mainDepth;
    Gfx::Image* depthCopy;
    std::vector<Light*> lights{};
    int mainLightIndex;
    InterleavedGradientNoise interleavedGradientNoise;

    Light* GetMainLight()
    {
        if (mainLightIndex >= 0 && mainLightIndex < lights.size())
            return lights[mainLightIndex];

        return nullptr;
    }
};
} // namespace Rendering
