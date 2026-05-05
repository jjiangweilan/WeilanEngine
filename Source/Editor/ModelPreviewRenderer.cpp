#include "Editor/ModelPreviewRenderer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/WeilanEngine.hpp"
#include <limits>

namespace
{
void ConfigureBlitPass(Gfx::RenderPass& pass)
{
    Gfx::SubpassAttachment color = {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment colors[] = {color};
    pass.SetSubpass(0, colors);
}

void ConfigurePreviewSettings(Rendering::RenderPipelineSetting& setting)
{
    setting.fxaa = false;
    setting.frustumCull = false;
    setting.shadowFrustumCull = false;
    setting.postProcess.colorGrading = false;
    setting.postProcess.bloom.enabled = false;
    setting.contactShadow.enabled = false;
    setting.ssao.enabled = false;
    setting.ssil.enabled = false;
    setting.rtgi.enabled = false;
    setting.gi.enabled = false;
}

void ExpandViewSpaceAABB(const glm::mat4& viewMatrix, const AABB& aabb, glm::vec3& outMinAABB, glm::vec3& outMaxAABB)
{
    glm::vec3 corners[] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z}
    };

    glm::vec3 center = glm::vec3(viewMatrix * glm::vec4((aabb.min + aabb.max) * 0.5f, 1.0f));
    for (const glm::vec3& corner : corners)
    {
        glm::vec3 viewCorner = glm::vec3(viewMatrix * glm::vec4(corner, 1.0f)) - center;
        outMinAABB = glm::min(outMinAABB, viewCorner);
        outMaxAABB = glm::max(outMaxAABB, viewCorner);
    }
}

void SetPreviewHierarchyEnabled(GameObject& gameObject)
{
    gameObject.SetWantsToBeEnabled();
    for (GameObject* child : gameObject.GetChildren())
    {
        if (child != nullptr)
        {
            SetPreviewHierarchyEnabled(*child);
        }
    }
}
} // namespace

ModelPreviewRenderer::ModelPreviewRenderer(WeilanEngine* engine) : engine(engine) {}

Gfx::Image* ModelPreviewRenderer::GetOrQueuePreview(const AssetPath& path)
{
    PreviewEntry* entry = GetEntry(path);
    if (entry != nullptr)
    {
        if (entry->failed)
        {
            return nullptr;
        }

        return entry->image.get();
    }

    if (!queued.contains(path))
    {
        pending.push(path);
        queued.insert(path);
    }

    return nullptr;
}

void ModelPreviewRenderer::RenderQueuedPreviews(Gfx::CommandBuffer& cmd, int maxPerFrame)
{
    for (int i = 0; i < maxPerFrame && !pending.empty(); ++i)
    {
        AssetPath path = pending.front();
        pending.pop();
        queued.erase(path);

        PreviewEntry* entry = CreateEntry(path);
        if (entry == nullptr || entry->failed)
        {
            continue;
        }

        if (!RenderPreview(*entry, cmd))
        {
            entry->failed = true;
        }
    }
}

void ModelPreviewRenderer::Invalidate(const AssetPath& path)
{
    previews.erase(path);
    queued.erase(path);
}

ModelPreviewRenderer::PreviewEntry* ModelPreviewRenderer::GetEntry(const AssetPath& path)
{
    auto iter = previews.find(path);
    if (iter == previews.end())
    {
        return nullptr;
    }

    return &iter->second;
}

ModelPreviewRenderer::PreviewEntry* ModelPreviewRenderer::CreateEntry(const AssetPath& path)
{
    auto [iter, inserted] = previews.try_emplace(path);
    PreviewEntry& entry = iter->second;
    if (!inserted)
    {
        return &entry;
    }

    if (!InitializePreview(path, entry))
    {
        entry.failed = true;
    }

    return &entry;
}

bool ModelPreviewRenderer::InitializePreview(const AssetPath& path, PreviewEntry& entry)
{
    Asset* asset = engine->assetDatabase->LoadAsset(path);
    Model* model = dynamic_cast<Model*>(asset);
    Prefab* prefab = dynamic_cast<Prefab*>(asset);
    if (model == nullptr && prefab == nullptr)
    {
        return false;
    }

    entry.image = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(PreviewSize, PreviewSize, Gfx::GfxFormat::R8G8B8A8_SRGB),
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );
    entry.image->SetName(path.string());

    entry.scene = std::make_unique<Scene>();
    entry.renderPipeline = std::make_unique<Rendering::RenderPipeline>();
    entry.renderPipelineSetting = std::make_unique<Rendering::RenderPipelineSetting>();
    ConfigurePreviewSettings(*entry.renderPipelineSetting);
    entry.scene->SetRenderPipelineSetting(entry.renderPipelineSetting.get());

    GameObject* cameraGO = entry.scene->CreateGameObject();
    cameraGO->SetName("ModelPreviewCamera");
    entry.camera = cameraGO->AddComponent<Camera>();
    entry.camera->SetFoV(glm::radians(50.0f));
    entry.camera->SetNear(0.01f);
    entry.camera->SetFar(1000.0f);
    entry.scene->SetMainCamera(entry.camera);

    entry.light = entry.scene->CreateGameObject();
    entry.light->SetName("ModelPreviewLight");
    Light* light = entry.light->AddComponent<Light>();
    light->SetLightType(LightType::Directional);
    light->SetIntensity(2.0f);
    light->SetLightColor({1.0f, 1.0f, 1.0f});

    if (model != nullptr)
    {
        SetupPreviewScene(*entry.scene, *model, entry);
    }
    else
    {
        SetupPreviewScene(*entry.scene, *prefab, entry);
    }

    const char* blitKeywords[] = {"_ResetAlpha"};
    entry.blitShader = ShaderLibrary::GetShader(
        "Blit",
        ShaderLibrary::QueryShaderFeatures("Blit").GetPermutation(blitKeywords)
    );
    entry.blitMaterial = std::make_unique<Material>();
    entry.blitMaterial->SetShader(entry.blitShader);
    ConfigureBlitPass(entry.blitPass);
    return true;
}

bool ModelPreviewRenderer::RenderPreview(PreviewEntry& entry, Gfx::CommandBuffer& cmd)
{
    Rendering::RenderConfig config = {
        .drawGraphics = false,
        .cmdOverride = &cmd
    };
    entry.renderPipeline->SetConfig(config);
    entry.renderPipeline->Render(*entry.scene, *entry.camera, {PreviewSize, PreviewSize});

    Gfx::Image* outputImage = GetGfxDriver()->GetImageFromRenderGraph(entry.renderPipeline->GetOutputColor());
    if (outputImage == nullptr)
    {
        return false;
    }

    entry.blitMaterial->SetTexture("input", outputImage);
    Gfx::ClearValue clear[] = {{0, 0, 0, 0}};
    entry.blitPass.SetAttachment(0, *entry.image);
    cmd.BeginRenderPass(entry.blitPass, clear);
    cmd.BindResource(0, entry.blitMaterial->GetShaderResource());
    cmd.BindShaderProgram(
        entry.blitShader->GetShaderProgram(),
        entry.blitShader->GetShaderProgram()->GetDefaultShaderConfig()
    );
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
    return true;
}

void ModelPreviewRenderer::SetupPreviewScene(Scene& scene, Model& model, PreviewEntry& entry)
{
    auto gameObjects = model.CreateGameObject();
    for (auto& gameObject : gameObjects)
    {
        gameObject->SetWantsToBeEnabled();
    }

    scene.AddGameObjects(std::move(gameObjects));
    FocusCamera(*entry.camera, scene);

    if (entry.light != nullptr)
    {
        glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.6f, -0.8f, -0.4f));
        entry.light->LookAt(lightDirection);
    }
}

void ModelPreviewRenderer::SetupPreviewScene(Scene& scene, Prefab& prefab, PreviewEntry& entry)
{
    if (prefab.GetGameObject() == nullptr)
    {
        FocusCamera(*entry.camera, scene);
        return;
    }

    std::unique_ptr<GameObject> gameObject = prefab.Instantiate();
    SetPreviewHierarchyEnabled(*gameObject);
    scene.AddGameObject(std::move(gameObject));
    FocusCamera(*entry.camera, scene);

    if (entry.light != nullptr)
    {
        glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.6f, -0.8f, -0.4f));
        entry.light->LookAt(lightDirection);
    }
}

void ModelPreviewRenderer::FocusCamera(Camera& camera, Scene& scene)
{
    auto renderers = scene.GetRenderingScene().GetGPUObjectRenderers();
    glm::vec3 minBounds = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    glm::vec3 maxBounds = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    bool hasRenderer = false;
    for (MeshRenderer* renderer : renderers)
    {
        if (renderer == nullptr || !renderer->IsActiveInScene())
        {
            continue;
        }

        hasRenderer = true;
        auto aabb = renderer->GetAABB();
        minBounds = glm::min(minBounds, aabb.min);
        maxBounds = glm::max(maxBounds, aabb.max);
    }

    glm::vec3 center = glm::vec3(0.0f);
    if (hasRenderer)
    {
        center = (minBounds + maxBounds) * 0.5f;
    }

    glm::vec3 previewDirection = glm::normalize(glm::vec3(0.55f, 0.35f, 0.75f));
    GameObject* cameraGO = camera.GetGameObject();
    cameraGO->SetPosition(center + previewDirection);
    cameraGO->LookAt(center - cameraGO->GetPosition());

    glm::vec3 minAABBV = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    glm::vec3 maxAABBV = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    glm::mat4 viewMatrix = camera.GetViewMatrix();
    if (hasRenderer)
    {
        for (MeshRenderer* renderer : renderers)
        {
            if (renderer == nullptr || !renderer->IsActiveInScene())
            {
                continue;
            }

            ExpandViewSpaceAABB(viewMatrix, renderer->GetAABB(), minAABBV, maxAABBV);
        }
    }
    else
    {
        ExpandViewSpaceAABB(viewMatrix, {center - 0.25f, center + 0.25f}, minAABBV, maxAABBV);
    }

    float halfWidth = glm::max(glm::abs(minAABBV.x), glm::abs(maxAABBV.x));
    float halfHeight = glm::max(glm::abs(minAABBV.y), glm::abs(maxAABBV.y));
    float halfDepth = glm::max(glm::abs(minAABBV.z), glm::abs(maxAABBV.z));
    float diagnalLength = glm::sqrt(halfWidth * halfWidth + halfHeight * halfHeight + halfDepth * halfDepth);
    diagnalLength = glm::max(diagnalLength * 1.05f, 1.0f);
    cameraGO->SetPosition(center - camera.GetForward() * diagnalLength);
}
