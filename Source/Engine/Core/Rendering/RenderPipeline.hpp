#pragma once

#include "Code/Ptr.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Component/Transform.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Core/Graphics/FrameContext.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
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
            void Init();
            ~RenderPipeline();

            void Render(RefPtr<GameScene> gameScene);
            inline void SetOffScreenOutput(bool offscreen) { offscreenOutput = offscreen; }
            RefPtr<Gfx::Image> GetOutputColor();
            RefPtr<Gfx::Image> GetOutputDepth();
            void ApplyAsset();
            RefPtr<RenderPipelineAsset> userConfigAsset;

#if GAME_EDITOR
            void RenderGameEditor(RefPtr<Editor::GameEditor> gameEditor);
#endif
        private:

            RefPtr<Gfx::GfxDriver> gfxDriver;
            UniPtr<RenderPipelineAsset> asset;

            void RenderObject(RefPtr<Transform> transform, UniPtr<CommandBuffer>& cmd);
            FrameContext* frameContext;

            UniPtr<Gfx::RenderPass> renderPass;
            UniPtr<Gfx::Image> colorImage;
            UniPtr<Gfx::Image> depthImage;
            UniPtr<Gfx::ShaderResource> globalResource;
            UniPtr<Gfx::Semaphore> imageAcquireSemaphore;
            RefPtr<CommandQueue> mainQueue;
            bool offscreenOutput;
            AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;

            void ProcessLights(RefPtr<GameScene> gameScene, RefPtr<CommandBuffer> cmdBuf);
    };
}
