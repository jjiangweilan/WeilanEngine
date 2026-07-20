#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Driver/GfxDriver/ShaderConfig.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/FogPass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/ReflectionProbeUpdate.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ContactShadow/ContactShadowPass.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/GrassSurfaceRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/PointLightShadowRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ShadowRenderer.hpp"
#include "Passes/BloomPass.hpp"
#include "Passes/CloudPass.hpp"
#include "Passes/ColorGradingPass.hpp"
#include "Passes/DepthDownSampler.hpp"
#include "Passes/FXAAPass.hpp"
#include "Passes/GI.hpp"
#include "Passes/HierarchyZBufferPass.hpp"
#include "Passes/LightingCombinePass.hpp"
#include "Passes/MotionVectorPass.hpp"
#include "Passes/PixelZoomPass.hpp"
#include "Passes/RTGI.hpp"
#include "Passes/SSAO.hpp"
#include "Passes/SSIL.hpp"
#include "Passes/ScreenSpaceShadowPass.hpp"
#include "Passes/Shader2HumanDebugPass.hpp"
#include "Passes/ShadingPass.hpp"
#include "Passes/TAAPass.hpp"
#include "PerScene.hpp"
#include "RenderEvents.hpp"
#include "RenderPipelineSetting.hpp"
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

    bool enablePixelZoom = false;
    glm::vec2 pixelZoomMousePos = {0, 0};
};

class RenderPipeline
{
    std::unique_ptr<GrassSurfaceRenderer> grassSurfaceRenderer;
    std::unique_ptr<ParticleRenderer> particleRenderer;
    std::unique_ptr<ShadowRenderer> shadowRenderer;
    std::unique_ptr<PointLightShadowRenderer> pointLightShadowRenderer;
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
    /**
     * @brief used as id for the final output color. Because GetOutputColor is returning a reference, I cached the value here. Maybe GetOutputColor should return by value
     */
    Gfx::ImageIdentifier finalColorId;

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
    Passes::TAAPass* taaPass;
    Passes::ScreenSpaceShadowPass* screenSpaceShadowPass;
    Passes::SSAO* ssaoPass;
    Passes::SSIL* ssilPass;
    Passes::RTGI* rtgiPass;
    Passes::GI* giPass;
    Passes::LightingCombinePass* lightingCombinePass;
    Passes::BloomPass* bloomPass;
    Passes::DepthDownSampler* depthDownSamplerPass;
    Passes::MotionVectorPass* motionVectorPass;
    Passes::HierarchyZBufferPass* hierarchyZBufferPass;
    SkyboxPass* skyboxPass;
    ContactShadowPass* contactShadowPass;
    Passes::PixelZoomPass* pixelZoomPass;
    Passes::Shader2HumanDebugPass* s2hDebugPass;

    std::unique_ptr<PipelineGPUBufferAllocator> bufferAllocator;

    struct FlatDrawInfo
    {
        Gfx::ShaderProgram* shaderProgram;
        const Gfx::PipelineConfig* pipelineConfig;
        MeshRenderer::MotionState* motionState = nullptr;
        size_t pipelineConfigHash;
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t firstInstance;
        uint32_t objectOffset;
        uint32_t previousSkeletonOffset = InvalidTextureIndex;
        bool castsShadows = true;
    };

    /**
     * @class DynamicObjectData
     * @brief data layout should match the data in GPU
     *
     */
    std::vector<GPUObjectShaderGroup> gpuObjectShaderGroups;
    std::vector<uint32_t> gpuObjectOffsets; // flat objectID array for all groups
    std::vector<FlatDrawInfo> flatDrawInfos;
    std::vector<DrawIndexedIndirectCommand> allIndirectCmds;
    std::vector<GpuDrawExtra> allIndirectCmdsExtra;
    std::vector<GPUDynamicMotionData> dynamicMotionDatas;
    uint32_t dynamicMotionDataOffset = InvalidTextureIndex;

    void BuildGPUObjectDrawData(Gfx::CommandBuffer& cmd, RenderingScene& renderingScene);
    void DrawGPUObjects(Gfx::CommandBuffer& cmd, std::optional<Gfx::PolygonMode> polygonModeOverride = std::nullopt, std::optional<Gfx::PipelineConfig::PipelineConfig_t::Stencil> stencilOverride = std::nullopt);

    struct ExecutionState
    {
        bool renderMainLightShadow = false;
    } state{};

    PerScene perScene;
    uint32_t frameIndex = 0;
    glm::vec2 currentTemporalJitterUv{0.0f};
    glm::vec2 previousTemporalJitterUv{0.0f};
    Camera* temporalJitterCamera = nullptr;

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
    Gfx::ShaderResource* GetPerSceneGPUResource() const { return perScene.GetGlobalResource(); }
    void SetRenderPipelineSetting(auto setting) { this->setting = setting; }

private:
    template <class T>
    T* AddRenderPipelinePass()
    {
        renderPipelinePasses.push_back(std::make_unique<T>());
        return static_cast<T*>(renderPipelinePasses.back().get());
    }
    bool FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize);
    void UpdateSceneInfo(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize);
    void BlitToFinalColor(Gfx::CommandBuffer* cmd);
    Gfx::ImageIdentifier GetFinalColor();
    Gfx::CommandBuffer* GetCommandBuffer();
    bool IsCommandBufferOverriden();
    void ExecuteRenderEvents(Gfx::CommandBuffer& cmd, Scene& scene, RenderEvents event); // new method for handling render events
                                                                                         //
    void SetupGPUDrivenBindings();
};
} // namespace Rendering
