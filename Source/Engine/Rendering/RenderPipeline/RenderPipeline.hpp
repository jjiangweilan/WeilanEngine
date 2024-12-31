#pragma once

#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Libs/Math.hpp"
#include "RenderPipelineSetting.hpp"
#include "Rendering/RenderingData.hpp"

namespace GPUParameter
{
using namespace glm;
#include "Shaders/DeferredPBRShadingInput.hlsl"
#include "Shaders/Library/PerScene.hlsl"
} // namespace GPUParameter

class Scene;
class Camera;

namespace Rendering
{

class RenderPipeline
{
public:
    RenderPipeline();

    void Render(Scene& scene, Camera& camera, glm::float2 screenSize);
    const Gfx::RG::ImageIdentifier& GetOutputColor() { return finalColor; }
    const Gfx::RG::ImageIdentifier& GetOutputDepth() { return mainDepth; }

    ObjPtr<RenderPipelineSetting> GetRenderPipelineSetting() const { return setting; }
    void SetRenderPipelineSetting(ObjPtr<RenderPipelineSetting> setting) { this->setting = setting; }

private:
    std::unique_ptr<Gfx::CommandBuffer> commandBuffer;

    Gfx::RG::ImageIdentifier mainColor = "mainColor";
    Gfx::RG::ImageIdentifier mainDepth = "mainDepth";
    Gfx::RG::ImageIdentifier albedoGBuffer = "albedoGBuffer";
    Gfx::RG::ImageIdentifier normalGBuffer = "normalGBuffer";
    Gfx::RG::ImageIdentifier maskGBuffer = "maskGBuffer";
    Gfx::RG::ImageIdentifier finalColor;

    Gfx::RG::ImageDescription mainColorDescription;
    Gfx::RG::ImageDescription mainDepthDescription;
    Gfx::RG::ImageDescription albedoGBufferDescription;
    Gfx::RG::ImageDescription normalGBufferDescription;
    Gfx::RG::ImageDescription maskGBufferDescription;

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

        bool updateMainLightShadow = true;

        const float shadowMapWidth = 1024.0f;
        const glm::float4 shadowMapTexelSize = {1 / shadowMapWidth, 1 / shadowMapWidth, shadowMapWidth, shadowMapWidth};

    } shadowMapPass{};

    struct ColorGradingPass
    {
        Gfx::RG::ImageIdentifier colorGradingId = Gfx::RG::ImageIdentifier("Color Grading");
    } colorGradingPass{};

    // WIP
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
    } labelColors;

    DrawList sceneDrawList;
    ObjPtr<RenderPipelineSetting> setting;
};
} // namespace Rendering
