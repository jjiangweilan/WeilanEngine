#pragma once
class Camera;
class Terrain;
namespace FrameGraph
{
struct RenderingData
{
    Camera* mainCamera;
    Terrain* terrain;
};
} // namespace FrameGraph
