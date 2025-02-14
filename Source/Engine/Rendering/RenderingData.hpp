#pragma once
#include "Rendering/DrawList.hpp"
#include <glm/glm.hpp>
namespace GPUParameter
{
using namespace glm;
#include "Shaders/DeferredPBRShadingInput.hlsl"
#include "Shaders/Library/PerScene.hlsl"
} // namespace GPUParameter
class Camera;
class Terrain;
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

struct RenderingData
{
    Camera* mainCamera;
    GPUParameter::PerScene* sceneInfo;
    Gfx::Image* mainColor;
    Gfx::Image* mainDepth;
};
} // namespace Rendering
