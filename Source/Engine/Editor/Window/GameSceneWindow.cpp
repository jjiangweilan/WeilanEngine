#include "GameSceneWindow.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Math/Geometry.hpp"
#include "Core/Model.hpp"
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Engine::Editor
{
glm::vec2 GetScreenUV(glm::vec4 gameViewRect)
{
    auto windowPos = ImGui::GetWindowPos();
    auto mousePos = ImGui::GetMousePos();
    glm::vec2 mouseInWindow{mousePos.x - windowPos.x, mousePos.y - windowPos.y};
    glm::vec2 gameViewSpace{mouseInWindow.x - gameViewRect.x, mouseInWindow.y - gameViewRect.y};
    glm::vec2 normalizedMouseInGameView = {gameViewSpace.x / gameViewRect.z, gameViewSpace.y / gameViewRect.w};

    return normalizedMouseInGameView;
}

struct ClickedGameObjectHelperStruct
{
    RefPtr<GameObject> go;
    float distance;
};

void EditTransform(glm::mat4& matrix, ImVec4 rect)
{

    static bool useSnap(false);
    // if (ImGui::IsKeyPressed(83)) useSnap = !useSnap;
    // ImGui::Checkbox("", &useSnap);
    // ImGui::SameLine();
    // vec_t snap;
    // switch (mCurrentGizmoOperation)
    // {
    //     case ImGuizmo::TRANSLATE:
    //         snap = config.mSnapTranslation;
    //         ImGui::InputFloat3("Snap", &snap.x);
    //         break;
    //     case ImGuizmo::ROTATE:
    //         snap = config.mSnapRotation;
    //         ImGui::InputFloat("Angle Snap", &snap.x);
    //         break;
    //     case ImGuizmo::SCALE:
    //         snap = config.mSnapScale;
    //         ImGui::InputFloat("Scale Snap", &snap.x);
    //         break;
    // }
}

void GetClickedGameObject(Ray ray, RefPtr<GameObject> root, std::vector<ClickedGameObjectHelperStruct>& clickedObjs)
{
    auto meshRenderer = root->GetComponent<MeshRenderer>();
    auto mesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;

    if (mesh)
    {
        float distance = std::numeric_limits<float>().infinity();
        for (auto& submesh : mesh->submeshes)
        {
            if (RayMeshIntersection(ray, submesh.Get(), root->GetTransform()->GetModelMatrix(), distance))
            {
                clickedObjs.push_back({root, distance});
            }
        }
    }

    for (auto& obj : root->GetTransform()->GetChildren())
    {
        GetClickedGameObject(ray, obj->GetGameObject(), clickedObjs);
    }
}

GameSceneWindow::GameSceneWindow(RefPtr<EditorContext> editorContext)
    : editorContext(editorContext), gameSceneCam(ProjectManagement::instance->GetLastEditorCameraPos())
{
    float camTheta, camPhi;
    ProjectManagement::instance->GetLastEditorCameraRotation(camTheta, camPhi);
    gameSceneCam.SetCameraRotation(camTheta, camPhi);
    outlineRawColor = AssetDatabase::Instance()->GetShader("Internal/OutlineRawColorPass");
    outlineFullScreen = AssetDatabase::Instance()->GetShader("Internal/OutlineFullScreenPass");
    blendBackShader = AssetDatabase::Instance()->GetShader("Internal/SimpleBlend");
    AssetDatabase::Instance()->RegisterOnAssetReload(
        [this](RefPtr<AssetObject> obj)
        {
            Shader* casted = dynamic_cast<Shader*>(obj.Get());
            if (casted && casted->GetName() == "Internal/OutlineRawColorPass")
            {
                this->outlineRawColor = casted;
            }
            else if (casted && casted->GetName() == "Internal/OutlineFullScreenPass")
            {
                this->outlineFullScreen = casted;
                outlineResource =
                    Gfx::GfxDriver::Instance()->CreateShaderResource(outlineFullScreen->GetShaderProgram(),
                                                                     Gfx::ShaderResourceFrequency::Shader);
                outlineResource->SetTexture("mainTex", outlineOffscreenColor);
            }
            else if (casted && casted->GetName() == "Internal/SimpleBlend")
            {
                this->blendBackShader = casted;
                blendBackResource =
                    Gfx::GfxDriver::Instance()->CreateShaderResource(blendBackShader->GetShaderProgram(),
                                                                     Gfx::ShaderResourceFrequency::Shader);
                blendBackResource->SetTexture("mainTex", editorOverlayColor);
            }
        });

    Gfx::ImageDescription imageDescription{.data = nullptr, // this pointer needs to be managed
                                           .width = GetGfxDriver()->GetSurfaceSize().width,
                                           .height = GetGfxDriver()->GetSurfaceSize().height,
                                           .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
                                           .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                                           .mipLevels = 1};
    gameSceneImage = GetGfxDriver()->CreateImage(imageDescription,
                                                 Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst |
                                                     Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferSrc);
    gameSceneImage->SetName("GameSceneWindow-GameSceneImage");

    gameSceneCam.Activate(false);
}

void GameSceneWindow::Tick()
{
    // UpdateRenderingResources(sceneColor, sceneDepth);

    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

    // show handle menu
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("Translate", "", activeHandle != nullptr))
        {
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
            /*if (activeHandle != nullptr && activeHandle->GetNameID() == "MoveSceneHandle")
            {
                activeHandle = nullptr;
            }
            else activeHandle = MakeUnique<MoveSceneHandle>();*/
        }
        if (ImGui::MenuItem("Rotate"))
        {
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
            /*if (activeHandle != nullptr && activeHandle->GetNameID() == "RotateSceneHandle")
            {
                activeHandle = nullptr;
            }
            else activeHandle = MakeUnique<RotateSceneHandle>();*/
        }
        if (ImGui::MenuItem("Scale"))
        {
            mCurrentGizmoOperation = ImGuizmo::SCALE;
            /*if (activeHandle != nullptr && activeHandle->GetNameID() == "ScaleSceneHandle")
            {
                activeHandle = nullptr;
            }
            activeHandle = MakeUnique<ScaleSceneHandle>();*/
        }
        const char* gameCam = "Game Camera";
        const char* editorCam = "Editor Camera";
        if (ImGui::MenuItem((gameSceneCam.IsActive() ? gameCam : editorCam), "v(V)", gameSceneCam.IsActive()))
        {
            if (gameSceneCam.IsActive())
                gameSceneCam.Deactivate();
            else
                gameSceneCam.Activate(false);
        }
        ImGui::EndMenuBar();
    }

    // draw game view
    ImVec4 gameViewRect;
    ImVec4 gizmoRect;
    {
        const auto& sceneDesc = gameSceneImage->GetDescription();
        float w = sceneDesc.width;
        float h = sceneDesc.height;
        float whRatio = sceneDesc.width / (float)sceneDesc.height;
        ImVec2 regMax = ImGui::GetWindowContentRegionMax();
        ImVec2 regMin = ImGui::GetWindowContentRegionMin();
        ImVec2 size = {regMax.x - regMin.x, regMax.y - regMin.y};

        // calculate size and pos
        float startingY = ImGui::GetCursorStartPos().y;
        float startingX = ImGui::GetCursorStartPos().x;
        if (w > size.x)
        {
            w = size.x;
            h = size.x / whRatio;
            ImGui::SetCursorPos(ImVec2(regMin.x - 1, startingY + ((regMax.y - startingY)) / 2.0f - h / 2.0f));
            gameViewRect.x = regMin.x - 1;
            gameViewRect.y = startingY + ((regMax.y - startingY)) / 2.0f - h / 2.0f;
        }
        if (h > size.y)
        {
            h = size.y;
            w = size.y * whRatio;
            ImGui::SetCursorPos(ImVec2(startingX + (regMax.x - regMin.x) / 2.0f - w / 2.0f, startingY - 2));
            gameViewRect.x = startingX + (regMax.x - regMin.x) / 2.0f - w / 2.0f;
            gameViewRect.y = startingY - 2;
        }
        size = ImVec2(w, h);
        gameViewRect.z = w;
        gameViewRect.w = h;

        gizmoRect = ImVec4(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y, size.x, size.y);

        ImGui::Image(gameSceneImage.Get(),
                     size,
                     ImVec2(0, 0),
                     ImVec2(1, 1),
                     ImVec4(1, 1, 1, 1),
                     ImVec4(0.3, 0.3, 0.3, 1));
    }
    glm::vec2 mouseInSceneViewUV =
        GetScreenUV(glm::vec4(gameViewRect.x, gameViewRect.y, gameViewRect.z, gameViewRect.w));

    // scene cam movement
    if (ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        if (!gameSceneCam.IsActive())
        {
            bool gameCamPos = ImGui::IsKeyDown(ImGuiKey_LeftShift);
            gameSceneCam.Activate(gameCamPos);
        }
        else
            gameSceneCam.Deactivate();
    }
    if (gameSceneCam.IsActive() && ImGui::IsWindowHovered())
    {
        gameSceneCam.Tick(mouseInSceneViewUV);
    }

    RefPtr<GameObject> go = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());

    if (go != nullptr)
    {
        auto matrix = go->GetTransform()->GetModelMatrix();
        ImGuizmo::SetRect(gizmoRect.x, gizmoRect.y, gizmoRect.z, gizmoRect.w);
        glm::mat4 viewMatrix = Camera::mainCamera->GetViewMatrix();
        glm::mat4 projectionMatrix = Camera::mainCamera->GetProjectionMatrix();
        projectionMatrix[1][1] = -projectionMatrix[1][1];
        ImGui::PushClipRect({gizmoRect.x, gizmoRect.y}, {gizmoRect.x + gizmoRect.z, gizmoRect.y + gizmoRect.w}, false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::Manipulate(glm::value_ptr(viewMatrix),
                             glm::value_ptr(projectionMatrix),
                             mCurrentGizmoOperation,
                             mCurrentGizmoMode,
                             glm::value_ptr(matrix),
                             NULL,
                             NULL); // useSnap ? &snap.x : NULL);
        ImGui::PopClipRect();
        go->GetTransform()->SetModelMatrix(matrix);
    }
    // pick object in scene
    if (ImGui::IsWindowFocused() && ImGui::IsMouseClicked(0) && !ImGuizmo::IsUsing() &&
        GameSceneManager::Instance()->GetActiveGameScene())
    {
        if (mouseInSceneViewUV.x > 0 && mouseInSceneViewUV.y > 0 && mouseInSceneViewUV.x < 1 &&
            mouseInSceneViewUV.y < 1)
        {
            std::vector<ClickedGameObjectHelperStruct> gameObjectsClicked;
            for (auto& g : GameSceneManager::Instance()->GetActiveGameScene()->GetRootObjects())
            {
                Ray ray = Camera::mainCamera->ScreenUVToWorldSpaceRay(mouseInSceneViewUV);
                GetClickedGameObject(ray, g, gameObjectsClicked);
            }
            if (gameObjectsClicked.size())
            {
                auto min = std::min_element(
                    gameObjectsClicked.begin(),
                    gameObjectsClicked.end(),
                    [](const ClickedGameObjectHelperStruct& left, const ClickedGameObjectHelperStruct& right)
                    { return left.distance < right.distance; });

                editorContext->currentSelected = min->go;
            }
            else
                editorContext->currentSelected = nullptr;
        }
    }
}

void GameSceneWindow::OnDestroy()
{
    ProjectManagement::instance->SetLastEditorCameraPos(gameSceneCam.GetCameraPos());
    float theta, phi;
    gameSceneCam.GetCameraRotation(theta, phi);
    ProjectManagement::instance->SetLastEditorCameraRotation(theta, phi);
}

void GameSceneWindow::UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth)
{

    if (outlineOffscreenColor == nullptr)
    {
        outlineOffscreenColor =
            Gfx::GfxDriver::Instance()->CreateImage(sceneColor->GetDescription(),
                                                    Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture);
    }

    if (outlineOffscreenDepth == nullptr)
    {
        auto desc = sceneDepth->GetDescription();
        desc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
        outlineOffscreenDepth = Gfx::GfxDriver::Instance()->CreateImage(desc, Gfx::ImageUsage::DepthStencilAttachment);
    }

    if (outlineResource == nullptr)
    {
        outlineResource = Gfx::GfxDriver::Instance()->CreateShaderResource(outlineFullScreen->GetShaderProgram(),
                                                                           Gfx::ShaderResourceFrequency::Shader);
        outlineResource->SetTexture("mainTex", outlineOffscreenColor);
    }

    if (newSceneColor == nullptr)
    {
        const Gfx::ImageDescription& newSceneColorDesc = sceneColor->GetDescription();
        newSceneColor =
            Gfx::GfxDriver::Instance()->CreateImage(newSceneColorDesc,
                                                    Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferDst |
                                                        Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
    }

    if (editorOverlayColor == nullptr)
    {
        const Gfx::ImageDescription& newSceneColorDesc = sceneColor->GetDescription();
        editorOverlayColor =
            Gfx::GfxDriver::Instance()->CreateImage(newSceneColorDesc,
                                                    Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferDst |
                                                        Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
    }

    if (editorOverlayDepth == nullptr)
    {
        Gfx::ImageDescription newDepthDesc = sceneColor->GetDescription();
        newDepthDesc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
        editorOverlayDepth =
            Gfx::GfxDriver::Instance()->CreateImage(newDepthDesc, Gfx::ImageUsage::DepthStencilAttachment);
    }

    if (outlinePass == nullptr)
    {
        outlinePass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment color;
        color.image = outlineOffscreenColor;
        color.loadOp = Gfx::AttachmentLoadOperation::Clear;
        color.storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::RenderPass::Attachment depth;
        depth.image = outlineOffscreenDepth;
        depth.loadOp = Gfx::AttachmentLoadOperation::Clear;
        depth.storeOp = Gfx::AttachmentStoreOperation::Store;
        depth.stencilLoadOp = Gfx::AttachmentLoadOperation::Clear;
        depth.stencilStoreOp = Gfx::AttachmentStoreOperation::Store;
        outlinePass->AddSubpass({color}, depth);
    }

    if (outlineFullScreenPass == nullptr)
    {
        /* scene pass data */
        outlineFullScreenPass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment sceneColorAttachment;
        sceneColorAttachment.image = editorOverlayColor;
        sceneColorAttachment.loadOp = Gfx::AttachmentLoadOperation::Clear;
        sceneColorAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::RenderPass::Attachment sceneDepthAttachment;
        sceneDepthAttachment.image = outlineOffscreenDepth;
        sceneDepthAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
        sceneDepthAttachment.storeOp = Gfx::AttachmentStoreOperation::DontCare;
        outlineFullScreenPass->AddSubpass({sceneColorAttachment}, sceneDepthAttachment);
    }

    if (handlePass == nullptr)
    {
        /* scene pass data */
        handlePass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment sceneColorAttachment;
        sceneColorAttachment.image = editorOverlayColor;
        sceneColorAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
        sceneColorAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::RenderPass::Attachment sceneDepthAttachment;
        sceneDepthAttachment.image = editorOverlayDepth;
        sceneDepthAttachment.loadOp = Gfx::AttachmentLoadOperation::Clear;
        sceneDepthAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
        handlePass->AddSubpass({sceneColorAttachment}, sceneDepthAttachment);
    }

    if (blendBackPass == nullptr)
    {
        blendBackPass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment colorAtta;
        colorAtta.image = newSceneColor;
        colorAtta.loadOp = Gfx::AttachmentLoadOperation::Load;
        colorAtta.storeOp = Gfx::AttachmentStoreOperation::Store;
        blendBackPass->AddSubpass({colorAtta}, std::nullopt);
    }

    if (blendBackResource == nullptr)
    {
        blendBackResource = Gfx::GfxDriver::Instance()->CreateShaderResource(blendBackShader->GetShaderProgram(),
                                                                             Gfx::ShaderResourceFrequency::Shader);
        blendBackResource->SetTexture("mainTex", editorOverlayColor);
    }
}

GameSceneCamera::GameSceneCamera(glm::vec3 initialPos)
{
    gameObject = MakeUnique<GameObject>();
    camera = gameObject->AddComponent<Camera>();
    lastActivePos = initialPos;
}

void GameSceneCamera::Activate(bool gameCamPos)
{
    oriCam = Camera::mainCamera;
    Camera::mainCamera = camera;
    if (gameCamPos)
    {
        camera->GetGameObject()->GetTransform()->SetPosition(oriCam->GetGameObject()->GetTransform()->GetPosition());
        camera->GetGameObject()->GetTransform()->SetRotation(
            oriCam->GetGameObject()->GetTransform()->GetRotationQuat());
    }
    else
    {
        camera->GetGameObject()->GetTransform()->SetPosition(lastActivePos);
    }
    isActive = true;
}

void GameSceneCamera::Tick(glm::vec2 mouseInSceneViewUV)
{
    auto transform = camera->GetGameObject()->GetTransform();
    switch (operationType)
    {
        case OperationType::Pan:
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                {
                    auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);

                    glm::vec3 xAxis = transform->GetModelMatrix()[0];
                    glm::vec3 yAxis = transform->GetModelMatrix()[1];

                    glm::vec3 newPos = transform->GetPosition() + xAxis * delta.x * 0.001f + yAxis * -delta.y * 0.001f;
                    transform->SetPosition(newPos);
                    ImGui::GetIO().MousePos = imguiInitialMousePos;
                    ImGui::GetIO().WantSetMousePos = true;
                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
                }
                else
                {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    SDL_WarpMouseGlobal(initialMousePos.x, initialMousePos.y);
                    operationType = OperationType::None;
                }
            }
            break;
        case OperationType::LookAround:
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    ImVec2 deltaLR = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                    phi += deltaLR.y / glm::two_pi<float>() / 20.0f;
                    theta += deltaLR.x / glm::two_pi<float>() / 20.0f;

                    UpdateRotation();

                    ImGui::GetIO().MousePos = imguiInitialMousePos;
                    ImGui::GetIO().WantSetMousePos = true;
                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
                }
                else
                {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    SDL_WarpMouseGlobal(initialMousePos.x, initialMousePos.y);
                    operationType = OperationType::None;
                }
            }
            break;
        case OperationType::None: break;
    }

    // zAxis Move
    float scroll = ImGui::GetIO().MouseWheel;
    if (scroll != 0)
    {
        glm::vec3 zAxis = transform->GetModelMatrix()[2];
        auto newPos = transform->GetPosition() + zAxis * -scroll;
        transform->SetPosition(newPos);
    }
    // right click move
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        glm::mat3 modelMatrix = transform->GetModelMatrix();
        glm::vec3 movement = glm::vec3(0);
        float speed = 0.1;
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
            speed = 0.8;
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
            speed = 0.4;

        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            movement -= modelMatrix[0] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            movement += modelMatrix[0] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            movement -= modelMatrix[2] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            movement += modelMatrix[2] * speed;
        }
        transform->SetPosition(transform->GetPosition() + movement);
    }

    bool isMouseWithInGameScene =
        mouseInSceneViewUV.x > 0 && mouseInSceneViewUV.x < 1 && mouseInSceneViewUV.y > 0 && mouseInSceneViewUV.y < 1;
    bool wantToPan = ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
    bool wantToLookAround = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if (isMouseWithInGameScene && (wantToPan || wantToLookAround))
    {
        if (wantToPan)
        {
            operationType = OperationType::Pan;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
        else if (wantToLookAround)
        {
            operationType = OperationType::LookAround;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        }
        SDL_GetGlobalMouseState(&initialMousePos.x, &initialMousePos.y);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        imguiInitialMousePos = ImGui::GetMousePos();
    }

    lastActivePos = transform->GetPosition();
}

void GameSceneCamera::Deactivate()
{
    Camera::mainCamera = oriCam;
    isActive = false;
}

} // namespace Engine::Editor
