#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Libs/Math.hpp"
#include "Modules/VolumetricCloud/Cloud.hpp"
#include "Passes/DepthDownSampler.hpp"
#include "Passes/SSAO.hpp"
#include "RenderEvents.hpp"
#include "RenderPipelineSetting.hpp"
#include "Rendering/RenderPipeline/Passes/ReflectionProbeUpdate.hpp"
#include "Rendering/Renderers/ContactShadow/ContactShadowPass.hpp"
#include "Rendering/Renderers/ShadowRenderer.hpp"
#include "Rendering/RenderingData.hpp"
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
    std::unique_ptr<Passes::ReflectionProbeUpdate> reflectionProbeUpdate;

    std::unique_ptr<Gfx::CommandBuffer> commandBuffer;

    Gfx::ImageIdentifier mainColor = "mainColor";
    Gfx::ImageIdentifier mainDepth = "mainDepth";
    Gfx::ImageIdentifier depthCopy = "depthCopy";
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

    struct ExecutionState
    {
        bool renderMainLightShadow = false;
    } state{};

    struct PerScene
    {
        PerScene();
        GPUParameter::Camera cameraParameter{};
        GPUParameter::Scene sceneParameter{};
        GPUParameter::MainLightShadow mainLightShadowParameter{};

        std::unique_ptr<Gfx::ShaderResource> globalResource{};

        std::unique_ptr<Gfx::Buffer> scene{};
        std::unique_ptr<Gfx::Buffer> camera{};
        std::unique_ptr<Gfx::Buffer> mainLightShadow{};
    } perScene{};

    struct ShadingPass
    {
        ShadingPass();

        GPUParameter::DeferredPBRShadingInput cpuParameter{};
        std::unique_ptr<Gfx::ShaderResource> gpuResource;
        std::unique_ptr<Gfx::Buffer> perMaterialBuffer;
        ObjPtr<Shader> shadingShader;

        Texture* brdfPreIntegeral;

        void UploadGPUParameter(Gfx::CommandBuffer& cmd);
    } shadingPass{};

    struct CloudPass
    {
        CloudPass();

        std::unique_ptr<Material> volumetricCloud = std::make_unique<Material>();
        inline static const char* volumetricCloudShader =
            "Source/Engine/Modules/VolumetricCloud/Shaders/VolumetricCloud";

        void Execute(Cloud& cloud, Gfx::CommandBuffer& cmd, RenderingData& renderingData);
    } cloudPass;

    struct ColorGradingPass
    {
        ColorGradingPass();
        Gfx::ImageIdentifier colorGradingId = Gfx::ImageIdentifier("Color Grading");
        Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("Color Grading");
        ObjPtr<Shader> colorGradingShader;
        Material mat;
    } colorGradingPass;

    SkyboxPass skyboxPass{};

    Passes::SSAO ssaoPass;

    struct FXAAPass
    {
        FXAAPass();

        Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
        Gfx::ImageIdentifier fxaaId = "FXAA";
        void Execute(
            Gfx::CommandBuffer& cmd,
            const glm::float4& sourceSize,
            const Gfx::ImageIdentifier& src,
            const Gfx::ImageIdentifier& dst
        );

        ObjPtr<Shader> shader;
        std::unique_ptr<Gfx::ShaderResource> resource;
    } fxaaPass{};

    struct ScreenSpaceShadow
    {
        ScreenSpaceShadow();

        ObjPtr<Shader> shader{};

    } screenSpaceShadowPass{};

    Passes::DepthDownSampler depthDownSamplerPass;

    struct
    {
        glm::float4 passColor = {0.2, 0.5, 0.1, 1.0};
        glm::float4 passColor1 = {0.5, 0.2, 0.6, 1.0};
    } labelColors;

    ObjPtr<RenderPipelineSetting> setting;
    RenderingData renderingData;
    Gfx::RenderPass skyboxOnlyPass = Gfx::RenderPass(1, 1);
    ContactShadowPass contactShadowPass; // new contact shadow pass (deferred insertion point)

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
    bool FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize);
    void UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize);
    void BlitToFinalColor(Gfx::CommandBuffer* cmd);
    Gfx::ImageIdentifier GetFinalColor();
    Gfx::CommandBuffer* GetCommandBuffer();
    bool IsCommandBufferOverriden();
    void ExecuteRenderEvents(Gfx::CommandBuffer& cmd, Scene& scene, RenderEvents event); // new method for handling render events
};
} // namespace Rendering
