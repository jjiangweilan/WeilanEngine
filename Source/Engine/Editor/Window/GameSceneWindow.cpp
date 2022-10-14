#include "GameSceneWindow.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Model.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Utils/Intersection.hpp"
#include "../imgui/imgui.h"
#include <algorithm>

namespace Engine::Editor
{
    class MoveSceneHandle : public GameSceneHandle
    {
        public:
            MoveSceneHandle()
            {
                RefPtr<Model> handleModel = AssetDatabase::Instance()->GetAssetFile("Assets/EngineInternal/GameEditor/Meshes/MoveHandle.glb")->GetRoot();
                mesh = handleModel ? handleModel->GetMesh("MoveHandle") : nullptr;
                handleShader = AssetDatabase::Instance()->GetShader("Internal/Handle3D");
            }
            void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override 
            {
                if (mesh && handleShader)
                {
                    Mesh::CommandBindMesh(cmdBuf, mesh);
                    cmdBuf->BindShaderProgram(handleShader->GetShaderProgram(), handleShader->GetDefaultShaderConfig());
                    cmdBuf->SetPushConstant(handleShader->GetShaderProgram(), &goModelMatrix);
                    cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
                }
            }
            void Interact(RefPtr<GameObject> go) override
            {
                auto mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                auto transform = go->GetTransform();
                glm::vec3 pos = transform->GetPosition();
                // go->GetTransform()->SetPostion();
                goModelMatrix = go->GetTransform()->GetModelMatrix();
            }
            std::string GetNameID() override { return "MoveSceneHandle"; }
        private:
            RefPtr<Shader> handleShader;
            RefPtr<Mesh> mesh;
            glm::mat4 goModelMatrix;
    };

    class RotateSceneHandle : public GameSceneHandle
    {
            void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override {};
            void Interact(RefPtr<GameObject> go) override {};
            std::string GetNameID() override { return "RotateSceneHandle"; }
    };

    class ScaleSceneHandle : public GameSceneHandle
    {
            void DrawHandle(RefPtr<CommandBuffer> cmdBuf) override {};
            void Interact(RefPtr<GameObject> go) override {};
            std::string GetNameID() override { return "ScaleSceneHandle"; }
    };

    struct ClickedGameObjectHelperStruct
    {
        RefPtr<GameObject> go;
        float distance;
    };

    void GetClickedGameObject(Utils::Ray ray, RefPtr<GameObject> root, std::vector<ClickedGameObjectHelperStruct>& clickedObjs)
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

        for(auto& obj : root->GetTransform()->GetChildren())
        {
            GetClickedGameObject(ray, obj->GetGameObject(), clickedObjs);
        }
    }

    GameSceneWindow::GameSceneWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext)
    {
        outlineByStencil = AssetDatabase::Instance()->GetShader("Internal/OutlineByStencil");
        outlineByStencilDrawOutline = AssetDatabase::Instance()->GetShader("Internal/OutlineByStencilDrawOutline");
    }

    void GameSceneWindow::Tick(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth)
    {
        UpdateRenderingResources(sceneColor, sceneDepth);
        this->sceneColor = sceneColor;
        this->sceneDepth = sceneDepth;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2, 0.2, 0.2, 1));
        ImGui::Begin("Game Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleColor();

        // show handle menu
        {
            ImGui::BeginMenuBar();

            if(ImGui::MenuItem("Move")) activeHandle = MakeUnique<MoveSceneHandle>();
            if(ImGui::MenuItem("Rotate")) activeHandle = MakeUnique<RotateSceneHandle>();
            if(ImGui::MenuItem("Scale")) activeHandle = MakeUnique<ScaleSceneHandle>();
            ImGui::EndMenuBar();
        }

        // draw game view
        ImVec4 gameViewRect;
        {
            const auto& sceneDesc = sceneColor->GetDescription();
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

            ImGui::Image(newSceneColor.Get(), size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(0.3, 0.3, 0.3, 1));
        }

        // pick object in scene
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && GameSceneManager::Instance()->GetActiveGameScene())
        {
            auto windowPos = ImGui::GetWindowPos();
            auto mousePos = ImGui::GetMousePos();
            glm::vec2 mouseInWindow { mousePos.x - windowPos.x, mousePos.y - windowPos.y };
            glm::vec2 gameViewSpace {mouseInWindow.x - gameViewRect.x, mouseInWindow.y - gameViewRect.y};
            glm::vec2 normalizedMouseInGameView = { gameViewSpace.x / gameViewRect.z, gameViewSpace.y / gameViewRect.w };

            if (normalizedMouseInGameView.x > 0 && normalizedMouseInGameView.y > 0 && normalizedMouseInGameView.x < 1 && normalizedMouseInGameView.y < 1)
            {
                std::vector<ClickedGameObjectHelperStruct> gameObjectsClicked;
                for(auto& g : GameSceneManager::Instance()->GetActiveGameScene()->GetRootObjects())
                {
                    glm::vec3 mouseInGameViewVS = glm::vec3((normalizedMouseInGameView - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{Camera::mainCamera->GetProjectionRight(), Camera::mainCamera->GetProjectionTop()}, -Camera::mainCamera->GetNear());
                    Utils::Ray ray;
                    ray.origin = Camera::mainCamera->GetGameObject()->GetTransform()->GetPosition();
                    glm::mat4 camModelMatrix = Camera::mainCamera->GetGameObject()->GetTransform()->GetModelMatrix();
                    glm::vec3 clickInWS = camModelMatrix * glm::vec4(mouseInGameViewVS, 1.0);
                    ray.direction = clickInWS - ray.origin;
                    GetClickedGameObject(ray, g, gameObjectsClicked);
                }
                if (gameObjectsClicked.size())
                {
                    auto min = std::min_element(
                        gameObjectsClicked.begin(),
                        gameObjectsClicked.end(),
                        [](const ClickedGameObjectHelperStruct& left, const ClickedGameObjectHelperStruct& right) { return left.distance < right.distance; });

                    editorContext->currentSelected = min->go;
                }
                else
                    editorContext->currentSelected = nullptr;
            }

        }
        RefPtr<GameObject> go = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());
        if (go != nullptr && activeHandle != nullptr)
        {
            activeHandle->Interact(go);
        }
        ImGui::End();
    }

    void GameSceneWindow::RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf)
    {
        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;

        GameObject* obj = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());
        cmdBuf->Blit(sceneColor, newSceneColor);
        cmdBuf->Blit(sceneDepth, newSceneDepth);
        cmdBuf->BeginRenderPass(scenePass, sceneFrameBuffer, clears);

        // draw handles
        if (obj != nullptr && activeHandle != nullptr)
        {
            activeHandle->DrawHandle(cmdBuf);
        }

        auto meshRenderer = obj ? obj->GetComponent<MeshRenderer>() : nullptr;
        auto mesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;
        auto material = meshRenderer ? meshRenderer->GetMaterial() : nullptr;
        // draw outline using stencil
        if (mesh != nullptr && material != nullptr)
        {
            cmdBuf->BindVertexBuffer(mesh->GetMeshBindingInfo().bindingBuffers, mesh->GetMeshBindingInfo().bindingOffsets, 0);
            cmdBuf->BindResource(material->GetShaderResource());
            cmdBuf->BindShaderProgram(outlineByStencil->GetShaderProgram(), outlineByStencil->GetDefaultShaderConfig());
            cmdBuf->BindIndexBuffer(mesh->GetMeshBindingInfo().indexBuffer.Get(), mesh->GetMeshBindingInfo().indexBufferOffset, mesh->GetIndexBufferType());
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
            cmdBuf->BindShaderProgram(outlineByStencilDrawOutline->GetShaderProgram(), outlineByStencilDrawOutline->GetDefaultShaderConfig());
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
        }

        cmdBuf->EndRenderPass();
    }

    void GameSceneWindow::UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth)
    {
        if (newSceneColor == nullptr)
        {
            const Gfx::ImageDescription& newSceneColorDesc = sceneColor->GetDescription();
            newSceneColor = Gfx::GfxDriver::Instance()->CreateImage(newSceneColorDesc, Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
        }

        if (newSceneDepth == nullptr)
        {
            Gfx::ImageDescription ourGameDepthDesc = sceneDepth->GetDescription();
            ourGameDepthDesc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
            newSceneDepth = Gfx::GfxDriver::Instance()->CreateImage(ourGameDepthDesc, Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::TransferDst);
        }

        if (scenePass == nullptr)
        {
            /* scene pass data */
            scenePass = Gfx::GfxDriver::Instance()->CreateRenderPass();
            Gfx::RenderPass::Attachment sceneDepthAttachment;
            sceneDepthAttachment.format = sceneDepth->GetDescription().format;
            sceneDepthAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
            sceneDepthAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
            Gfx::RenderPass::Attachment sceneColorAttachment;
            sceneColorAttachment.format = sceneColor->GetDescription().format;
            sceneColorAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
            sceneColorAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
            scenePass->SetAttachments({sceneColorAttachment}, sceneDepthAttachment);
        }

        if (sceneFrameBuffer == nullptr)
        {
            Gfx::RenderPass::Subpass editorPass0;
            editorPass0.colors.push_back(0);
            editorPass0.depthAttachment = 1;
            scenePass->SetSubpass({editorPass0});
            sceneFrameBuffer = Gfx::GfxDriver::Instance()->CreateFrameBuffer(scenePass);
            sceneFrameBuffer->SetAttachments({newSceneColor, newSceneDepth});
        }
    }

}
