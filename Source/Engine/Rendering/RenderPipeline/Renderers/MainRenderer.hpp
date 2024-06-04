#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "MainRendererData.hpp"
#include "Passes/DeferredShading.hpp"
#include "Passes/GBufferPass.hpp"
#include "Rendering/RenderingData.hpp"

namespace Rendering
{
class MainRenderer
{
public:
    MainRenderer();

    void Execute(RenderingContext& renderContext, RenderingData& renderingData);

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
    PassWrapper<DeferredShading> deferredShading;

    struct ShaderGlobal
    {
        float time;
    } shaderGlobal;

    MainRendererData rendererData;

    void ProcessLights(Scene& gameScene);
    void CreateCameraImages();

    void Setup(Gfx::CommandBuffer& cmd, RenderingData& frameData);
    void SetupFrame(Gfx::CommandBuffer& cmd, RenderingData& frameData);
    void PassesSetup(Gfx::CommandBuffer& cmd, RenderingData& frameData);
};
} // namespace Rendering
