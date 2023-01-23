#include "GameSceneWindow.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Math/Geometry.hpp"
#include "Core/Model.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <SDL2/SDL.h>
#include <algorithm>
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

class MoveSceneHandle : public GameSceneHandle
{
public:
    MoveSceneHandle()
    {
        RefPtr<Model> handleModel = AssetDatabase::Instance()
                                        ->GetAssetFile("Assets/EngineInternal/GameEditor/Meshes/MoveHandle.glb")
                                        ->GetRoot();
        mesh = handleModel ? handleModel->GetMesh("MoveHandle") : nullptr;
        handleShader = AssetDatabase::Instance()->GetShader("Internal/Handle3D");
    }
    void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override
    {
        if (mesh && handleShader)
        {
            // Mesh::CommandBindMesh(cmdBuf, mesh);
            cmdBuf->BindShaderProgram(handleShader->GetShaderProgram(), handleShader->GetDefaultShaderConfig());
            cmdBuf->SetPushConstant(handleShader->GetShaderProgram(), &goModelMatrix);
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
        }
    }
    void Interact(RefPtr<GameObject> go, glm::vec2 mouseInSceneViewUVSpace) override
    {
        goModelMatrix = go->GetTransform()->GetModelMatrix();
        Ray ray = Camera::mainCamera->ScreenUVToWorldSpaceRay(mouseInSceneViewUVSpace);
        if (mesh)
        {
            float distance;
            glm::vec3 p0, p1, p2;
            if (inFlight.isPressing == false && ImGui::IsMouseClicked(0) &&
                RayMeshIntersection(ray, mesh, go->GetTransform()->GetModelMatrix(), distance, p0, p1, p2))
            {
                inFlight.moveDir = {0, 0, 0};
                // just take any trianle vertex to see which axis it belongs to
                if (p0.x > p0.y && p0.x > p0.z)
                {
                    inFlight.moveDir = goModelMatrix[0];
                }
                else if (p0.y > p0.x && p0.y > p0.z)
                {
                    inFlight.moveDir = goModelMatrix[1];
                }
                else if (p0.z > p0.x && p0.z > p0.y)
                {
                    inFlight.moveDir = goModelMatrix[2];
                }
                inFlight.moveDir = glm::normalize(inFlight.moveDir);
                inFlight.startPos = go->GetTransform()->GetPosition();
                inFlight.planeNorm =
                    glm::cross(glm::vec3(Camera::mainCamera->GetGameObject()->GetTransform()->GetModelMatrix()[1]),
                               inFlight.moveDir);
                Ray ray = Camera::mainCamera->ScreenUVToWorldSpaceRay(mouseInSceneViewUVSpace);
                float distance;
                if (glm::intersectRayPlane(ray.origin, ray.direction, inFlight.startPos, inFlight.planeNorm, distance))
                {
                    glm::vec3 p = ray.origin + ray.direction * distance;
                    glm::vec3 newPos = ClosestPointOnLine(p, inFlight.startPos, inFlight.moveDir);
                    inFlight.offset = newPos - inFlight.startPos;
                    inFlight.isPressing = true;
                }
                lastMouseInViewUVSpace = mouseInSceneViewUVSpace;
                return;
            }
        }

        if (inFlight.isPressing == true)
        {
            const glm::vec3 planeNorm = inFlight.planeNorm;
            glm::vec3 planeOrigin = inFlight.startPos;
            Ray ray = Camera::mainCamera->ScreenUVToWorldSpaceRay(mouseInSceneViewUVSpace);
            float distance;
            if (glm::intersectRayPlane(ray.origin, ray.direction, planeOrigin, planeNorm, distance))
            {
                glm::vec3 p = ray.origin + ray.direction * distance;
                glm::vec3 newPos = ClosestPointOnLine(p, inFlight.startPos, inFlight.moveDir) - inFlight.offset;
                go->GetTransform()->SetPosition(newPos);
            }
        }

        if (inFlight.isPressing == true && ImGui::IsMouseReleased(0))
        {
            inFlight.isPressing = false;
            return;
        }
    }
    std::string GetNameID() override { return "MoveSceneHandle"; }

private:
    RefPtr<Shader> handleShader;
    RefPtr<Mesh> mesh;
    glm::mat4 goModelMatrix;
    glm::vec2 lastMouseInViewUVSpace;

    struct InFlightData
    {
        bool isPressing = false;
        glm::vec3 startPos;
        glm::vec3 moveDir;
        glm::vec3 planeNorm;
        glm::vec3 offset;
    } inFlight;

    glm::vec3 ClosestPointOnLine(glm::vec3 p0, glm::vec3 l0, glm::vec3 l)
    {
        float dotln = dot(l, p0 - l0);
        if (dotln == 0) return glm::vec3(0);

        return l0 + l * dotln;
    }
};

class RotateSceneHandle : public GameSceneHandle
{
    void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override{};
    void Interact(RefPtr<GameObject> go, glm::vec2 mouseInSceneViewUVSpace) override{};
    std::string GetNameID() override { return "RotateSceneHandle"; }
};

class ScaleSceneHandle : public GameSceneHandle
{
    void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override{};
    void Interact(RefPtr<GameObject> go, glm::vec2 mouseInSceneViewUVSpace) override{};
    std::string GetNameID() override { return "ScaleSceneHandle"; }
};

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
        if (RayMeshIntersection(ray, mesh, root->GetTransform()->GetModelMatrix(), distance))
        {
            clickedObjs.push_back({root, distance});
        }
    }

    for (auto& obj : root->GetTransform()->GetChildren())
    {
        GetClickedGameObject(ray, obj->GetGameObject(), clickedObjs);
    }
}

GameSceneWindow::GameSceneWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext)
{
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
                                           .width = GetGfxDriver()->GetWindowSize().width,
                                           .height = GetGfxDriver()->GetWindowSize().height,
                                           .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
                                           .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                                           .mipLevels = 1};
    gameSceneImage = GetGfxDriver()->CreateImage(imageDescription,
                                                 Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst |
                                                     Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferSrc);
    gameSceneImage->SetName("GameSceneWindow-GameSceneImage");
}

void GameSceneWindow::Tick()
{
    // UpdateRenderingResources(sceneColor, sceneDepth);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2, 0.2, 0.2, 1));
    ImGui::Begin("Game Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleColor();

    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

    // show handle menu
    {
        ImGui::BeginMenuBar();

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
            if (gameSceneCam.IsActive()) gameSceneCam.Deactivate();
            else gameSceneCam.Activate(false);
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
        else gameSceneCam.Deactivate();
    }
    if (gameSceneCam.IsActive())
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
        ImGuizmo::Manipulate(glm::value_ptr(viewMatrix),
                             glm::value_ptr(projectionMatrix),
                             mCurrentGizmoOperation,
                             mCurrentGizmoMode,
                             glm::value_ptr(matrix),
                             NULL,
                             NULL); // useSnap ? &snap.x : NULL);
        go->GetTransform()->SetModelMatrix(matrix);
    }
    // pick object in scene
    if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsUsing() && GameSceneManager::Instance()->GetActiveGameScene())
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
            else editorContext->currentSelected = nullptr;
        }
    }
    ImGui::End();
}

void GameSceneWindow::RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf)
{
    if (sceneColor == nullptr) return;
    static std::vector<Gfx::ClearValue> clears(2);
    clears[0].color = {{0, 0, 0, 0}};
    clears[1].depthStencil.depth = 1;

    GameObject* obj = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());
    cmdBuf->Blit(sceneColor, newSceneColor);

    // draw outline
    auto meshRenderer = obj ? obj->GetComponent<MeshRenderer>() : nullptr;
    auto mesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;
    auto material = meshRenderer ? meshRenderer->GetMaterial() : nullptr;
    bool outlineDrew = false;
    cmdBuf->BeginRenderPass(outlinePass, clears);

    if (mesh != nullptr && material != nullptr)
    {
        cmdBuf->BindVertexBuffer(mesh->GetMeshBindingInfo().bindingBuffers, 0);
        cmdBuf->BindShaderProgram(outlineRawColor->GetShaderProgram(), outlineRawColor->GetDefaultShaderConfig());
        cmdBuf->BindIndexBuffer(mesh->GetMeshBindingInfo().indexBuffer.Get(),
                                mesh->GetMeshBindingInfo().indexBufferOffset,
                                mesh->GetIndexBufferType());
        auto modelMatrix = obj->GetTransform()->GetModelMatrix();
        cmdBuf->SetPushConstant(material->GetShader()->GetShaderProgram(), &modelMatrix);
        cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);

        cmdBuf->EndRenderPass();

        cmdBuf->BeginRenderPass(outlineFullScreenPass, clears);

        cmdBuf->BindShaderProgram(outlineFullScreen->GetShaderProgram(), outlineFullScreen->GetDefaultShaderConfig());
        cmdBuf->BindResource(outlineResource);
        cmdBuf->Draw(6, 1, 0, 0);
        outlineDrew = true;
    }
    cmdBuf->EndRenderPass();

    cmdBuf->BeginRenderPass(handlePass, clears);
    // draw handles
    if (obj != nullptr && activeHandle != nullptr && obj != Camera::mainCamera->GetGameObject().Get())
    {
        activeHandle->DrawHandle(cmdBuf);
    }
    cmdBuf->EndRenderPass();

    if (outlineDrew)
    {
        cmdBuf->BeginRenderPass(blendBackPass, clears);
        cmdBuf->BindShaderProgram(blendBackShader->GetShaderProgram(), blendBackShader->GetDefaultShaderConfig());
        cmdBuf->BindResource(blendBackResource);
        cmdBuf->Draw(6, 1, 0, 0);
        cmdBuf->EndRenderPass();
    }
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

GameSceneCamera::GameSceneCamera()
{
    gameObject = MakeUnique<GameObject>();
    camera = gameObject->AddComponent<Camera>();
}

void GameSceneCamera::Activate(bool gameCamPos)
{
    oriCam = Camera::mainCamera;
    Camera::mainCamera = camera;
    if (initialActive)
    {
        camera->GetGameObject()->GetTransform()->SetPosition(oriCam->GetGameObject()->GetTransform()->GetPosition());
        camera->GetGameObject()->GetTransform()->SetRotation(
            oriCam->GetGameObject()->GetTransform()->GetRotationQuat());
        initialActive = false;
    }
    else if (gameCamPos)
    {
        camera->GetGameObject()->GetTransform()->SetPosition(oriCam->GetGameObject()->GetTransform()->GetPosition());
        camera->GetGameObject()->GetTransform()->SetRotation(
            oriCam->GetGameObject()->GetTransform()->GetRotationQuat());
    }
    else
    {
        camera->GetGameObject()->GetTransform()->SetPosition(lastActivePos);
        camera->GetGameObject()->GetTransform()->SetRotation(lastActiveRotation);
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
                    auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                    glm::vec3 xAxis = transform->GetModelMatrix()[0];
                    glm::mat4 rotation = glm::rotate(glm::mat4(1), glm::radians(-delta.x * 0.05f), glm::vec3(0, 1, 0));
                    rotation = glm::rotate(rotation, glm::radians(-delta.y * 0.05f), xAxis);

                    auto finalRot = rotation * glm::mat4_cast(transform->GetRotationQuat());
                    transform->SetRotation(finalRot);
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
        float speed = 0.8;
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
    lastActiveRotation = transform->GetRotationQuat();
}

void GameSceneCamera::Deactivate()
{
    Camera::mainCamera = oriCam;
    isActive = false;
}

} // namespace Engine::Editor
