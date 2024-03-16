#pragma once
#include <glm/glm.hpp>
class Camera;
class Terrain;
namespace FrameGraph
{
struct RenderingData
{
    glm::ivec2 screenSize;
    Camera* mainCamera;
    Terrain* terrain;
};
} // namespace FrameGraph
