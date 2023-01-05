#pragma once

#include "Libs/Ptr.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Graphics/FrameContext.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "RenderGraph/RenderGraph.hpp"

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
    RefPtr<Texture> virtualTexture;

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
                AssetDatabase::Instance()->GetShader("Internal/SceneLayout")->GetShaderProgram(),
                Gfx::ShaderResourceFrequency::Global);
            assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload([this](RefPtr<AssetObject> obj)
                                                                                     { this->OnAssetReload(obj); });
        }

        ~GlobalShaderResource() { AssetDatabase::Instance()->UnregisterOnAssetReload(assetReloadIterHandle); }

        struct Vals
        {
            glm::mat4 view;
            glm::mat4 projection;
            glm::mat4 viewProjection;
            glm::vec3 viewPos;
        } v;

        UniPtr<Gfx::ShaderResource>& GetShaderResource() { return shaderResource; }
        void QueueCommand(RefPtr<CommandBuffer> cmdBuf);
        void OnAssetReload(RefPtr<AssetObject> obj)
        {
            Shader* casted = dynamic_cast<Shader*>(obj.Get());
            if (casted && casted->GetName() == "Internal/SceneLayout")
            {
                this->shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                    AssetDatabase::Instance()->GetShader("Internal/SceneLayout")->GetShaderProgram(),
                    Gfx::ShaderResourceFrequency::Global);
            }
        }

    private:
        UniPtr<Gfx::ShaderResource> shaderResource;
        AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;

    } globalShaderResoruce;

    RefPtr<Gfx::GfxDriver> gfxDriver;
    UniPtr<RenderPipelineAsset> asset;

    void RenderObject(RefPtr<Transform> transform, UniPtr<CommandBuffer>& cmd, std::vector<RGraph::DrawData>& drawList);
    FrameContext* frameContext;

    UniPtr<Gfx::RenderPass> renderPass;
    UniPtr<Gfx::Image> colorImage;
    UniPtr<Gfx::Image> depthImage;
    UniPtr<Gfx::Semaphore> imageAcquireSemaphore;
    UniPtr<Gfx::Semaphore> mainCmdBufFinishedSemaphore;
    UniPtr<Gfx::Fence> mainCmdBufFinishedFence;
    UniPtr<Gfx::CommandPool> cmdPool;
    UniPtr<CommandBuffer> cmdBuf;
    RefPtr<CommandQueue> mainQueue;
#if GAME_EDITOR
    RefPtr<Editor::GameEditorRenderer> gameEditorRenderer;
    bool renderEditor;
#endif

    RGraph::RenderGraph graph;
    RGraph::ImageNode* colorImageNode;
    RGraph::ImageNode* swapChainImageNode;
    RGraph::ImageNode* depthImageNode;
    RGraph::RenderPassNode* renderPassNode;
    RGraph::BlitNode* blitNode;
    RGraph::GPUBarrierNode* swapChainToPesentNode;

    void PrepareFrameData(RefPtr<CommandBuffer> cmdBuf);
    void ProcessLights(RefPtr<GameScene> gameScene, RefPtr<CommandBuffer> cmdBuf);
};
} // namespace Engine::Rendering
