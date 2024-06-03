#pragma once
#include "Core/Component/Camera.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Passes/GBufferPass.hpp"
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

    Gfx::RG::ImageIdentifier mainColor;
    Gfx::RG::ImageIdentifier mainDepth;

    GBufferPass gbufferPass;

    struct ShaderGlobal
    {
        float time;
    } shaderGlobal;

    void ProcessLights(Scene& gameScene);
    void CreateCameraImages();
    void SetupFrame(Camera& camera, Gfx::CommandBuffer& cmd);
};
} // namespace Rendering
