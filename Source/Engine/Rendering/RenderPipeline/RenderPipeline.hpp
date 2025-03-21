#pragma once

#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Libs/Math.hpp"
#include "RenderPipelineSetting.hpp"
#include "Rendering/RenderingData.hpp"
#include "SkyboxPass.hpp"

class Scene;
class Camera;

namespace Rendering
{
using namespace RenderPasses;

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

    void SetConfig(const RenderConfig& config) { this->renderConfig = config; }
    void Render(Scene& scene, Camera& camera, glm::float2 screenSize);
    void RenderSkyboxOnly(Scene& scene, Camera& camera, glm::float2 screenSize);
    const auto& GetOutputColor() const { return finalColor; }
    const auto& GetOutputDepth() const { return mainDepth; }
    auto GetRenderPipelineSetting() const { return setting; }
    Gfx::ShaderResource* GetPerSceneGPUResource() const { return perScene.gpuResourceSet.get(); }

    void SetRenderPipelineSetting(auto setting) { this->setting = setting; }

private:
    std::unique_ptr<Gfx::CommandBuffer> commandBuffer;

    Gfx::RG::ImageIdentifier mainColor = "mainColor";
    Gfx::RG::ImageIdentifier mainDepth = "mainDepth";
    Gfx::RG::ImageIdentifier depthCopy = "depthCopy";
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
        GPUParameter::PerScene cpuParameter{};
        std::unique_ptr<Gfx::Buffer> gpuBuffer{};
        std::unique_ptr<Gfx::ShaderResource> gpuResourceSet{};
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
        GPUParameter::DeferredPBRShadingInput cpuParameter;
        std::unique_ptr<Gfx::ShaderResource> gpuResource;
        std::unique_ptr<Gfx::Buffer> perMaterialBuffer;
        ObjPtr<Shader2> shadingShader;

        Texture* brdfPreIntegeral;

        void UploadGPUParameter(Gfx::CommandBuffer& cmd);
    } shadingPass{};

    struct ShadowMapPass
    {
        ShadowMapPass();

        Gfx::RG::RenderPass pass = Gfx::RG::RenderPass(1, 1);
        Gfx::RG::ImageIdentifier shadowMapId;
        Gfx::ImageDescription shadowDescription;
        std::unique_ptr<Gfx::Image> shadowMap;
        ObjPtr<Shader2> shadowMapShader;
        ObjPtr<Shader2> shadowMapShaderSkinned;

        bool updateMainLightShadow = true;

        const float shadowMapWidth = 1024.0f;
        const glm::float4 shadowMapTexelSize = {1 / shadowMapWidth, 1 / shadowMapWidth, shadowMapWidth, shadowMapWidth};

    } shadowMapPass{};

    struct ColorGradingPass
    {
        ColorGradingPass();
        Gfx::RG::ImageIdentifier colorGradingId = Gfx::RG::ImageIdentifier("Color Grading");
        Gfx::RG::RenderPass pass = Gfx::RG::RenderPass::SingleColor("Color Grading");
        ObjPtr<Shader2> colorGradingShader;
        Material mat;
    } colorGradingPass;

    SkyboxPass skyboxPass{};

    struct AmbientOcclusionPass
    {
        AmbientOcclusionPass();
        ObjPtr<Shader2> ssaoShader;
        Material mat;
        Gfx::RG::ImageIdentifier ssao = Gfx::RG::ImageIdentifier("SSAO");

        Gfx::RG::RenderPass pass = Gfx::RG::RenderPass::SingleColor("SSAO");
        void Execute(
            Gfx::CommandBuffer* cmd, Gfx::Image* texDepth, RenderPipelineSetting* setting, RenderingData& renderingData
        );
    } ambientOcclusionPass;

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
