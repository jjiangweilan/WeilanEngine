#pragma once
#include "GfxDriver/Vulkan/VKDriver.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Code/Ptr.hpp"
#include "Core/Rendering/RenderPipeline.hpp"

#if GAME_EDITOR
#include "Editor/GameEditor.hpp"
#endif
namespace Engine
{
    class WeilanEngine
    {
        public:
            ~WeilanEngine();
            void Launch();

        private:
            UniPtr<Gfx::VKDriver> vkDriver;

            // when no camera is in the world, engine uses it's own virtual camera
            void SetVirtualCamera();
            std::shared_ptr<RenderTarget> virtualRenderTarget;
            UniPtr<AssetDatabase> assetDatabase;
            UniPtr<Rendering::RenderPipeline> renderPipeline;
#if GAME_EDITOR
            UniPtr<Editor::GameEditor> gameEditor;
#endif

            void RegisterAssetImporters();
            void ConfigureProjectPath();
    };
}
