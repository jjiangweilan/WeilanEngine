#pragma once
#include "Core/Component/Camera.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/RenderingData.hpp"
namespace Rendering
{
class MainRenderer
{
public:
    MainRenderer();

    void Setup(Camera& camera, Gfx::CommandBuffer& cmd);
    void Execute(Gfx::CommandBuffer& cmd);

private:
    SceneInfo sceneInfo;
    RenderingData renderingData;

    std::unique_ptr<Gfx::Buffer> sceneInfoBuffer;
    std::unique_ptr<Gfx::Buffer> shaderGlobalBuffer;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    struct ShaderGlobal
    {
        float time;
    } shaderGlobal;

    void ProcessLights(Scene& gameScene);
};
} // namespace Rendering
