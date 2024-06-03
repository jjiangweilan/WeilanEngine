#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "MainRendererData.hpp"
#include "Passes/GBufferPass.hpp"
#include "Rendering/FrameData.hpp"

namespace Rendering
{
class MainRenderer
{
public:
    MainRenderer();

    void Execute(Gfx::CommandBuffer& cmd, FrameData& frameData);

private:
    SceneInfo sceneInfo;

    std::unique_ptr<Gfx::Buffer> sceneInfoBuffer;
    std::unique_ptr<Gfx::Buffer> shaderGlobalBuffer;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    Gfx::RG::ImageIdentifier mainColor;
    Gfx::RG::ImageIdentifier mainDepth;

    template <class T>
    struct PassWrapper
    {
        T pass;
        T::Setting setting;
    };

    PassWrapper<GBufferPass> gbuffer;

    struct ShaderGlobal
    {
        float time;
    } shaderGlobal;

    MainRendererData rendererData;

    void ProcessLights(Scene& gameScene);
    void CreateCameraImages();

    void Setup(Gfx::CommandBuffer& cmd, FrameData& frameData);
    void SetupFrame(Gfx::CommandBuffer& cmd, FrameData& frameData);
    void PassesSetup(Gfx::CommandBuffer& cmd, FrameData& frameData);
};
} // namespace Rendering
