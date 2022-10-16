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
        outlineRawColor = AssetDatabase::Instance()->GetShader("Internal/OutlineRawColorPass");
        outlineFullScreen = AssetDatabase::Instance()->GetShader("Internal/OutlineFullScreenPass");
        blendBackShader = AssetDatabase::Instance()->GetShader("Internal/SimpleBlend");
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
        static std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;

        GameObject* obj = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());
        cmdBuf->Blit(sceneColor, newSceneColor);

        // draw outline
        auto meshRenderer = obj ? obj->GetComponent<MeshRenderer>() : nullptr;
        auto mesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;
        auto material = meshRenderer ? meshRenderer->GetMaterial() : nullptr;
        cmdBuf->BeginRenderPass(outlinePass, clears);

        if (mesh != nullptr && material != nullptr)
        {
            cmdBuf->BindVertexBuffer(mesh->GetMeshBindingInfo().bindingBuffers, mesh->GetMeshBindingInfo().bindingOffsets, 0);
            cmdBuf->BindShaderProgram(outlineRawColor->GetShaderProgram(), outlineRawColor->GetDefaultShaderConfig());
            cmdBuf->BindIndexBuffer(mesh->GetMeshBindingInfo().indexBuffer.Get(), mesh->GetMeshBindingInfo().indexBufferOffset, mesh->GetIndexBufferType());
            auto modelMatrix = obj->GetTransform()->GetModelMatrix();
            cmdBuf->SetPushConstant(material->GetShader()->GetShaderProgram(), &modelMatrix);
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);

            cmdBuf->EndRenderPass();

            cmdBuf->BeginRenderPass(outlineFullScreenPass, clears);

            cmdBuf->BindShaderProgram(outlineFullScreen->GetShaderProgram(), outlineFullScreen->GetDefaultShaderConfig());
            cmdBuf->BindResource(outlineResource);
            cmdBuf->Draw(6, 1, 0, 0);
        }
        cmdBuf->EndRenderPass();

        cmdBuf->BeginRenderPass(handlePass, clears);
        // draw handles
        if (obj != nullptr && activeHandle != nullptr)
        {
            activeHandle->DrawHandle(cmdBuf);
        }
        cmdBuf->EndRenderPass();

        cmdBuf->BeginRenderPass(blendBackPass, clears);
        cmdBuf->BindShaderProgram(blendBackShader->GetShaderProgram(), blendBackShader->GetDefaultShaderConfig());
        cmdBuf->BindResource(blendBackResource);
        cmdBuf->Draw(6, 1, 0, 0);
        cmdBuf->EndRenderPass();
    }

    void GameSceneWindow::UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth)
    {

        if (outlineOffscreenColor == nullptr)
        {
            outlineOffscreenColor = Gfx::GfxDriver::Instance()->CreateImage(sceneColor->GetDescription(), Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture);
        }

        if (outlineOffscreenDepth == nullptr)
        {
            auto desc = sceneDepth->GetDescription();
            desc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
            outlineOffscreenDepth = Gfx::GfxDriver::Instance()->CreateImage(desc, Gfx::ImageUsage::DepthStencilAttachment);
        }

        if (outlineResource == nullptr)
        {
            outlineResource = Gfx::GfxDriver::Instance()->CreateShaderResource(outlineFullScreen->GetShaderProgram(), Gfx::ShaderResourceFrequency::Shader);
            outlineResource->SetTexture("mainTex", outlineOffscreenColor);
        }

        if (newSceneColor == nullptr)
        {
            const Gfx::ImageDescription& newSceneColorDesc = sceneColor->GetDescription();
            newSceneColor = Gfx::GfxDriver::Instance()->CreateImage(newSceneColorDesc, Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
        }

        if (editorOverlayColor == nullptr)
        {
            const Gfx::ImageDescription& newSceneColorDesc = sceneColor->GetDescription();
            editorOverlayColor = Gfx::GfxDriver::Instance()->CreateImage(newSceneColorDesc, Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
        }

        if (editorOverlayDepth == nullptr)
        {
            Gfx::ImageDescription newDepthDesc = sceneColor->GetDescription();
            newDepthDesc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
            editorOverlayDepth = Gfx::GfxDriver::Instance()->CreateImage(newDepthDesc, Gfx::ImageUsage::DepthStencilAttachment);
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

        if(blendBackPass == nullptr)
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
            blendBackResource = Gfx::GfxDriver::Instance()->CreateShaderResource(blendBackShader->GetShaderProgram(), Gfx::ShaderResourceFrequency::Shader);
            blendBackResource->SetTexture("mainTex", editorOverlayColor);
        }

    }

}
