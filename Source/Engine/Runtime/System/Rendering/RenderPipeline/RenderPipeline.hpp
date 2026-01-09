#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Passes/CloudPass.hpp"
#include "Passes/ColorGradingPass.hpp"
#include "Passes/DepthDownSampler.hpp"
#include "Passes/FXAAPass.hpp"
#include "Passes/SSAO.hpp"
#include "Passes/BloomPass.hpp"
#include "Passes/ScreenSpaceShadowPass.hpp"
#include "Passes/ShadingPass.hpp"
#include "PerScene.hpp"
#include "RenderEvents.hpp"
#include "RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/FogPass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/ReflectionProbeUpdate.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ContactShadow/ContactShadowPass.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ShadowRenderer.hpp"
#include "SkyboxPass.hpp"

class Scene;
class Camera;

namespace Rendering
{
using namespace RenderPasses;
class ParticleRenderer;

class SceneRendererSorter
{
public:
    void operator()(Scene& scene, Camera& camera, Rendering::DrawList& outDrawList);
};

struct RenderConfig
{
    /** draw cmds dispatched by the Graphics API */
    bool drawGraphics = false;

    std::optional<Gfx::CommandBuffer*> cmdOverride;

    /** override the color output of the render pipeline */
    std::optional<Gfx::ImageView*> colorOutputOverride;
};

class RenderPipeline
{
    std::unique_ptr<ParticleRenderer> particleRenderer;
    std::unique_ptr<ShadowRenderer> shadowRenderer;
    std::unique_ptr<Passes::FogPass> fogPass;

    std::unique_ptr<Gfx::CommandBuffer> commandBuffer;

    Gfx::ImageIdentifier mainColor = "mainColor";
    Gfx::ImageIdentifier mainDepth = "mainDepth";
    Gfx::ImageIdentifier depthCopy = "depthCopy";
    Gfx::ImageIdentifier colorCopy = "colorCopy";
    Gfx::ImageIdentifier downSampledDepthCopy = "downSampledDepthCopy";
    Gfx::ImageIdentifier albedoGBuffer = "albedoGBuffer";
    Gfx::ImageIdentifier normalGBuffer = "normalGBuffer";
    Gfx::ImageIdentifier maskGBuffer = "maskGBuffer";
    Gfx::ImageIdentifier finalColor;

    Gfx::RenderImageDescriptor mainColorDescription;
    Gfx::RenderImageDescriptor mainDepthDescription;
    Gfx::RenderImageDescriptor albedoGBufferDescription;
    Gfx::RenderImageDescriptor normalGBufferDescription;
    Gfx::RenderImageDescriptor maskGBufferDescription;

    RenderConfig renderConfig;
    std::vector<std::unique_ptr<RenderPipelinePass>> renderPipelinePasses;

    Passes::ReflectionProbeUpdate* reflectionProbeUpdate;
    Passes::ShadingPass* shadingPass;
    Passes::CloudPass* cloudPass;
    Passes::ColorGradingPass* colorGradingPass;
    Passes::FXAAPass* fxaaPass;
    Passes::ScreenSpaceShadowPass* screenSpaceShadowPass;
    Passes::SSAO* ssaoPass;
    Passes::BloomPass* bloomPass;
    Passes::DepthDownSampler* depthDownSamplerPass;
    SkyboxPass* skyboxPass;
    ContactShadowPass* contactShadowPass;

    std::unique_ptr<PipelineGPUBufferAllocator> bufferAllocator;

    struct ExecutionState
    {
        bool renderMainLightShadow = false;
    } state{};

    PerScene perScene;

    struct
    {
        glm::float4 passColor = {0.2, 0.5, 0.1, 1.0};
        glm::float4 passColor1 = {0.5, 0.2, 0.6, 1.0};
    } labelColors;

    ObjPtr<RenderPipelineSetting> setting;
    RenderingData renderingData;
    Gfx::RenderPass skyboxOnlyPass = Gfx::RenderPass(1, 1);

public:
    RenderPipeline();
    ~RenderPipeline();

    void SetConfig(const RenderConfig& config) { this->renderConfig = config; }
    void Render(Scene& scene, Camera& camera, glm::float2 screenSize);
    void RenderSkyboxOnly(Scene& scene, Camera& camera, glm::float2 screenSize);

    const Gfx::ImageIdentifier& GetOutputColor();
    const auto& GetOutputDepth() { return mainDepth; }
    auto GetRenderPipelineSetting() const { return setting; }
    Gfx::ShaderResource* GetPerSceneGPUResource() const { return perScene.globalResource.get(); }
    void SetRenderPipelineSetting(auto setting) { this->setting = setting; }

private:
    template <class T>
    T* AddRenderPipelinePass()
    {
        renderPipelinePasses.push_back(std::make_unique<T>());
        return static_cast<T*>(renderPipelinePasses.back().get());
    }
    bool FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize);
    void UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize);
    void BlitToFinalColor(Gfx::CommandBuffer* cmd);
    Gfx::ImageIdentifier GetFinalColor();
    Gfx::CommandBuffer* GetCommandBuffer();
    bool IsCommandBufferOverriden();
    void ExecuteRenderEvents(Gfx::CommandBuffer& cmd, Scene& scene, RenderEvents event); // new method for handling render events
};
} // namespace Rendering
