#pragma once
#include "Code/Ptr.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Rendering/RenderPipeline.hpp"
#include "GfxDriver/Vulkan/VKDriver.hpp"

#if GAME_EDITOR
#include "Editor/GameEditor.hpp"
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#endif
namespace Engine
{
class WeilanEngine
{
public:
    ~WeilanEngine();
    void Launch();

private:
    RefPtr<Gfx::GfxDriver> gfxDriver;

    UniPtr<AssetDatabase> assetDatabase;
    UniPtr<Rendering::RenderPipeline> renderPipeline;
#if GAME_EDITOR
    UniPtr<Editor::ProjectManagement> projectManagement;
    UniPtr<Editor::GameEditor> gameEditor;
#endif

    void RegisterAssetImporters();
};
} // namespace Engine
