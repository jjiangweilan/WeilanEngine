#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Libs/Math.hpp"
#include "Modules/VolumetricCloud/Cloud.hpp"
#include "Passes/DepthDownSampler.hpp"
#include "Passes/SSAO.hpp"
#include "RenderPipelineSetting.hpp"
#include "Rendering/RenderPipeline/Passes/ReflectionprobeUpdate.hpp"
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
public:
    RenderPipeline();
    ~RenderPipeline();

    void SetConfig(const RenderConfig& config) { this->renderConfig = config; }
    void Render(Scene& scene, Camera& camera, glm::float2 screenSize);
    void RenderSkyboxOnly(Scene& scene, Camera& camera, glm::float2 screenSize);
    const auto& GetOutputColor() const { return finalColor; }
    const auto& GetOutputDepth() const { return mainDepth; }
    auto GetRenderPipelineSetting() const { return setting; }
    Gfx::ShaderResource* GetPerSceneGPUResource() const { return perScene.globalResource.get(); }

    void SetRenderPipelineSetting(auto setting) { this->setting = setting; }

private:
    std::unique_ptr<ParticleRenderer> particleRenderer;
    std::unique_ptr<ShadowRenderer> shadowRenderer;
    std::unique_ptr<Passes::ReflectionProbeUpdate> reflectionProbeUpdate;

    std::unique_ptr<Gfx::CommandBuffer> commandBuffer;

    Gfx::RG::ImageIdentifier mainColor = "mainColor";
    Gfx::RG::ImageIdentifier mainDepth = "mainDepth";
    Gfx::RG::ImageIdentifier depthCopy = "depthCopy";
    Gfx::RG::ImageIdentifier downSampledDepthCopy = "downSampledDepthCopy";
    Gfx::RG::ImageIdentifier albedoGBuffer = "albedoGBuffer";
    Gfx::RG::ImageIdentifier normalGBuffer = "normalGBuffer";
    Gfx::RG::ImageIdentifier maskGBuffer = "maskGBuffer";
    Gfx::RG::ImageIdentifier finalColor;

    Gfx::RG::ImageDescription mainColorDescription;
    Gfx::RG::ImageDescription mainDepthDescription;
    Gfx::RG::ImageDescription albedoGBufferDescription;
    Gfx::RG::ImageDescription normalGBufferDescription;
    Gfx::RG::ImageDescription maskGBufferDescription;

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

    struct GBufferPass
    {
        GBufferPass();
        Gfx::RG::RenderPass pass;
    } gbufferPass{};

    struct ForwardPass
    {
        ForwardPass();
        Gfx::RG::RenderPass pass;
    } forwardPass{};

    struct ShadingPass
    {
        ShadingPass();

        Gfx::RG::RenderPass pass;
        GPUParameter::DeferredPBRShadingInput cpuParameter{};
        std::unique_ptr<Gfx::ShaderResource> gpuResource;
        std::unique_ptr<Gfx::Buffer> perMaterialBuffer;
        ObjPtr<Shader2> shadingShader;

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
        Gfx::RG::ImageIdentifier colorGradingId = Gfx::RG::ImageIdentifier("Color Grading");
        Gfx::RG::RenderPass pass = Gfx::RG::RenderPass::SingleColor("Color Grading");
        ObjPtr<Shader2> colorGradingShader;
        Material mat;
    } colorGradingPass;

    SkyboxPass skyboxPass{};

    Passes::SSAO ssaoPass;

    struct FXAAPass
    {
        FXAAPass();

        Gfx::RG::RenderPass pass = Gfx::RG::RenderPass(1, 1);
        Gfx::RG::ImageIdentifier fxaaId = "FXAA";
        void Execute(
            Gfx::CommandBuffer& cmd,
            const glm::float4& sourceSize,
            const Gfx::RG::ImageIdentifier& src,
            const Gfx::RG::ImageIdentifier& dst
        );

        ObjPtr<Shader2> shader;
        std::unique_ptr<Gfx::ShaderResource> resource;
    } fxaaPass{};

    struct ScreenSpaceShadow
    {
        ScreenSpaceShadow();

        ObjPtr<Shader2> shader{};

    } screenSpaceShadowPass{};

    Passes::DepthDownSampler depthDownSamplerPass;

    struct
    {
        glm::float4 passColor = {0.2, 0.5, 0.1, 1.0};
        glm::float4 passColor1 = {0.5, 0.2, 0.6, 1.0};
    } labelColors;

    ObjPtr<RenderPipelineSetting> setting;
    RenderingData renderingData;

    Gfx::RG::RenderPass skyboxOnlyPass = Gfx::RG::RenderPass(1, 1);

    bool FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize);
    void UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize);
    void BlitToFinalColor(Gfx::CommandBuffer* cmd);
    Gfx::RG::ImageIdentifier GetFinalColor();
    Gfx::CommandBuffer* GetCommandBuffer();
    bool IsCommandBufferOverriden();
};
} // namespace Rendering
