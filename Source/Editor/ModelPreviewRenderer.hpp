#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

class WeilanEngine;
class Model;
class GameObject;
class Camera;
class Shader;

namespace Gfx
{
class CommandBuffer;
class Image;
}

class ModelPreviewRenderer
{
public:
    explicit ModelPreviewRenderer(WeilanEngine* engine);

    Gfx::Image* GetOrQueuePreview(const AssetPath& path);
    void RenderQueuedPreviews(Gfx::CommandBuffer& cmd, int maxPerFrame = 1);
    void Invalidate(const AssetPath& path);

private:
    struct PreviewEntry
    {
        std::unique_ptr<Gfx::Image> image;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<Rendering::RenderPipeline> renderPipeline;
        std::unique_ptr<Rendering::RenderPipelineSetting> renderPipelineSetting;
        std::unique_ptr<Material> blitMaterial;
        ObjPtr<Shader> blitShader;
        Gfx::RenderPass blitPass = Gfx::RenderPass(1, 1);
        Camera* camera = nullptr;
        GameObject* light = nullptr;
        bool failed = false;
    };

    static constexpr uint32_t PreviewSize = 128;

    WeilanEngine* engine;
    std::unordered_map<AssetPath, PreviewEntry> previews;
    std::queue<AssetPath> pending;
    std::unordered_set<AssetPath> queued;

    PreviewEntry* GetEntry(const AssetPath& path);
    PreviewEntry* CreateEntry(const AssetPath& path);
    bool InitializePreview(const AssetPath& path, PreviewEntry& entry);
    bool RenderPreview(PreviewEntry& entry, Gfx::CommandBuffer& cmd);
    void SetupPreviewScene(Scene& scene, Model& model, PreviewEntry& entry);
    void FocusCamera(Camera& camera, Scene& scene);
};
