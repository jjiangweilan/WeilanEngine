#pragma once

#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Transform.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Ptr.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "VirtualTexture/VirtualTextureRenderer.hpp"

#if GAME_EDITOR
#include "Editor/GameEditor.hpp"
#endif
namespace Engine
{
class GameScene;
}
namespace Engine::Rendering
{
class RenderPipelineAsset : public AssetObject
{
public:
    RenderPipelineAsset();
    // UniPtr<VirtualTexture> virtualTexture;

private:
};

class RenderPipeline
{
public:
    RenderPipeline(RefPtr<Gfx::GfxDriver> gfxDriver);
    void Init(RefPtr<Editor::GameEditorRenderer> gameEditorRenderer);
    ~RenderPipeline();

    void Render(RefPtr<GameScene> gameScene);
    void ApplyAsset();
    RefPtr<RenderPipelineAsset> userConfigAsset;

private:
    // represent the global shader resource in the shader
    // directly set the value of `v`
    // data will be updated later in the pipeline
    struct GlobalShaderResource
    {
        GlobalShaderResource()
        {
            shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                AssetDatabase::Instance()->GetShader(ShaderSceneLayout)->GetShaderProgram(),
                frequency);
            assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload([this](RefPtr<AssetObject> obj)
                                                                                     { this->OnAssetReload(obj); });
        }

        ~GlobalShaderResource() { AssetDatabase::Instance()->UnregisterOnAssetReload(assetReloadIterHandle); }

        static const int MAX_LIGHT_COUNT = 32; // defined in Commom.glsl
        struct Light
        {
            glm::vec4 position;
            float range;
            float intensity;
        };

        struct SceneInfo
        {
            glm::vec4 viewPos;
            glm::mat4 view;
            glm::mat4 projection;
            glm::mat4 viewProjection;
            glm::mat4 worldToShadow;
            glm::vec4 lightCount;
            Light lights[MAX_LIGHT_COUNT];
        } sceneInfo;

        UniPtr<Gfx::ShaderResource>& GetShaderResource() { return shaderResource; }
        void OnAssetReload(RefPtr<AssetObject> obj)
        {
            Shader* casted = dynamic_cast<Shader*>(obj.Get());
            if (casted && casted->GetName() == ShaderSceneLayout)
            {
                this->shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                    AssetDatabase::Instance()->GetShader(ShaderSceneLayout)->GetShaderProgram(),
                    frequency);
            }
        }

    private:
        UniPtr<Gfx::ShaderResource> shaderResource;
        AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;
        const char* ShaderSceneLayout = "Internal/SceneLayout";
        const Gfx::ShaderResourceFrequency frequency = Gfx::ShaderResourceFrequency::Global;

    } sceneResource;

    struct ShadowPassResource
    {
        struct SceneShadow
        {
            glm::mat4 worldToShadow;
        } sceneShadow;
        ShadowPassResource()
        {
            shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                AssetDatabase::Instance()->GetShader(ShaderShadowMap)->GetShaderProgram(),
                frequency);
            assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload([this](RefPtr<AssetObject> obj)
                                                                                     { this->OnAssetReload(obj); });
        }

        ~ShadowPassResource() { AssetDatabase::Instance()->UnregisterOnAssetReload(assetReloadIterHandle); }

        UniPtr<Gfx::ShaderResource> shaderResource;
        const char* ShaderShadowMap = "Game/ShadowMap";

    private:
        void OnAssetReload(RefPtr<AssetObject> obj)
        {
            Shader* casted = dynamic_cast<Shader*>(obj.Get());
            if (casted && casted->GetName() == ShaderShadowMap)
            {
                this->shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                    AssetDatabase::Instance()->GetShader(ShaderShadowMap)->GetShaderProgram(),
                    frequency);
            }
        }

        AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;

        const Gfx::ShaderResourceFrequency frequency = Gfx::ShaderResourceFrequency::Global;
    } shadowResource;

    RefPtr<Gfx::GfxDriver> gfxDriver;
    UniPtr<RenderPipelineAsset> asset;

    void AppendDrawData(RefPtr<Transform> transform, std::vector<RGraph::DrawData>& drawList);
    void CompileGraph();

    UniPtr<Gfx::Semaphore> imageAcquireSemaphore;
    UniPtr<Gfx::Semaphore> mainCmdBufFinishedSemaphore;
    UniPtr<Gfx::Semaphore> resourceTransferFinishedSemaphore;
    UniPtr<Gfx::Fence> mainCmdBufFinishedFence;
    UniPtr<Gfx::CommandPool> cmdPool;
    UniPtr<CommandBuffer> cmdBuf;
    UniPtr<CommandBuffer> resourceTransferCmdBuf;
    RefPtr<CommandQueue> mainQueue;
#if GAME_EDITOR
    RefPtr<Editor::GameEditorRenderer> gameEditorRenderer;
    bool renderEditor;
#endif

    RGraph::RenderGraph graph;

    // shadow map
    RGraph::ImageNode* shadowMapDepth;
    RGraph::RenderPassNode* shadowPassNode;

    RGraph::ImageNode* vtCacheTexNode;
    RGraph::ImageNode* vtIndirTexNode;
    RGraph::ImageNode* colorImageNode;
    RGraph::ImageNode* swapChainImageNode;
    RGraph::ImageNode* depthImageNode;
    RGraph::RenderPassNode* renderPassNode;
    RGraph::BlitNode* blitNode;
    RGraph::GPUBarrierNode* swapChainToPesentNode;

    RGraph::DrawList sceneDrawList;
    UniPtr<VirtualTexture> vt;

    void PrepareFrameData(RefPtr<CommandBuffer> cmdBuf);
    using MainLight = Light;
    MainLight* ProcessLights(RefPtr<GameScene> gameScene);

    /* features */
    VirtualTextureRenderer virtualTextureRenderer;
    Libs::Image::LinearImage* vtCacheTex = nullptr;
    Libs::Image::LinearImage* vtIndirTex = nullptr;
    RGraph::BufferNode* vtCacheTexStagingBuffer;
    RGraph::BufferNode* vtIndirTexStagingBuffer;
};
} // namespace Engine::Rendering
