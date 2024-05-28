#pragma once
#include <glm/glm.hpp>
class Camera;
class Terrain;
namespace FrameGraph
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
    LightInfo lights[MAX_LIGHT_COUNT];
};

struct RenderingData
{
    glm::ivec2 screenSize;
    Camera* mainCamera;
    Terrain* terrain;
    SceneInfo* sceneInfo;
};
} // namespace FrameGraph
