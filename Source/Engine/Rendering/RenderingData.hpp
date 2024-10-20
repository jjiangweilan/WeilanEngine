#pragma once
#include "Rendering/DrawList.hpp"
#include <glm/glm.hpp>
class Camera;
class Terrain;
namespace Rendering
{
static const int MAX_LIGHT_COUNT = 128; // defined in Commom.glsl

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

struct SceneInfo
{
    glm::vec4 viewPos;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::mat4 worldToShadow;
    glm::mat4 invProjection;
    glm::mat4 invNDCToWorld;
    float lightCount;
    float padding0;
    float padding1;
    float padding2;
    glm::vec4 shadowMapSize;
    glm::vec4 cameraZBufferParams;
    glm::vec4 cameraFrustum;
    glm::vec4 screenSize;
    glm::vec4 cachedMainLightDirection;
    LightInfo lights[MAX_LIGHT_COUNT];
};

struct RenderingData
{
    bool updateMainLightShadow = false;
    glm::ivec2 screenSize;
    Camera* mainCamera;
    Terrain* terrain;
    RenderingScene* renderingScene;
    SceneInfo* sceneInfo;
    DrawList* drawList;
    Gfx::CommandBuffer* cmd;
};
} // namespace Rendering
