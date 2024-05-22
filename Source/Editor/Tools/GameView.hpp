#pragma once
#include "../Tool.hpp"
#include "Asset/Shader.hpp"
#include "Core/Gizmo.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include <list>

namespace Editor
{
class GameView : public Tool
{
public:
    GameView();
    ~GameView() override;

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"View", "Game"};
    }

    void Init();

    bool Tick() override;

    void Render(
        Gfx::CommandBuffer& cmd,
        const Gfx::RG::ImageIdentifier* gameImage,
        const Gfx::RG::ImageIdentifier* gameDepthImage
    );

    Camera* GetEditorCamera() const
    {
        return editorCamera;
    }

    Gfx::Image* GetSceneImage()
    {
        return sceneImage.get();
    }

private:
    std::unique_ptr<Gfx::Image> sceneImage;

    // scene image can't be deleted immediately because it's tracked by the VKDriver for at least two frame (it holds a
    // pointer and doesn't check its validity)
    struct PendingDelete
    {
        std::unique_ptr<Gfx::Image> image;
        int frameCount = 0;
    };
    std::list<PendingDelete> pendingDeleteSceneImages;
    std::unique_ptr<GameObject> editorCameraGO;
    Gfx::Image* graphOutputImage = nullptr;
    bool useViewCamera = true;

    struct
    {
        glm::ivec2 resolution;
    } d; // data

    bool firstFrame = true;
    Camera* gameCamera = nullptr;
    Camera* editorCamera = nullptr;
    Gfx::RG::ImageIdentifier outlineSrcRT;
    Gfx::RG::RenderPass outlineSrcPass = Gfx::RG::RenderPass::SingleColor();
    Gfx::RG::RenderPass gameImagePass = Gfx::RG::RenderPass::Default(
        "gameImage pass",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );
    Shader* outlineRawColorPassShader;
    Shader* outlineFullScreenPassShader;

    void CreateRenderData(uint32_t width, uint32_t height);
    void EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& proj, const glm::vec4& rect);
    void ChangeGameScreenResolution(glm::ivec2 resolution);
    void FocusOnObject(Camera& camera, GameObject& gameObject);
};
} // namespace Editor
