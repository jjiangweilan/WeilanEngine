#include "GameEditor.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui.h"

#include "Core/GameScene/GameScene.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "EditorRegister.hpp"
#include "Extension/Inspector/BuildInInspectors.hpp"
namespace Engine::Editor
{
    GameEditor::GameEditor(RefPtr<Gfx::GfxDriver> gfxDriver) : gfxDriver(gfxDriver)
    {
    }

    GameEditor::~GameEditor()
    {
        projectManagement->Save();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void GameEditor::ConfigEditorPath()
    {
        projectManagement = MakeUnique<ProjectManagement>();
        ProjectManagement::instance = projectManagement;
        projectManagement->RecoverLastProject();
    }

    void GameEditor::LoadCurrentProject()
    {
        projectManagement->LoadProject();
    }

    void GameEditor::Init(RefPtr<Rendering::RenderPipeline> renderPipeline)
    {
        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForVulkan(gfxDriver->GetSDLWindow());
        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
        editorContext = MakeUnique<EditorContext>();
        sceneTreeWindow = MakeUnique<SceneTreeWindow>(editorContext);
        inspector = MakeUnique<InspectorWindow>(editorContext);
        assetExplorer = MakeUnique<AssetExplorer>(editorContext);
        gameSceneWindow = MakeUnique<GameSceneWindow>(editorContext);
        projectWindow = nullptr;

        InitializeBuiltInInspector();

        imGuiData.indexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(1024, Gfx::BufferUsage::Index, true);
        imGuiData.vertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(1024, Gfx::BufferUsage::Vertex, true);
        imGuiData.shaderProgram = AssetDatabase::Instance()->GetShader("ImGui")->GetShaderProgram();
        imGuiData.generalShaderRes = Gfx::GfxDriver::Instance()->CreateShaderResource(imGuiData.shaderProgram, Gfx::ShaderResourceFrequency::Global);

        // imGuiData.editorRT creation
        {
            Gfx::ImageDescription editorRTDesc;
            editorRTDesc.width = gfxDriver->GetWindowSize().width;
            editorRTDesc.height = gfxDriver->GetWindowSize().height;
            editorRTDesc.format = Gfx::ImageFormat::R8G8B8A8_UNorm;
            imGuiData.editorRT = Gfx::GfxDriver::Instance()->CreateImage(editorRTDesc, Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferSrc);
        }

        // imGuiData.fontTex creation
        Gfx::ImageDescription fontTexDesc;
        int width, height;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(
                &fontTexDesc.data,
                &width,
                &height);
        fontTexDesc.width = width;
        fontTexDesc.height = height;
        fontTexDesc.format = Gfx::ImageFormat::R8G8B8A8_UNorm;
        imGuiData.fontTex = Gfx::GfxDriver::Instance()->CreateImage(fontTexDesc, Gfx::ImageUsage::Texture);
        imGuiData.generalShaderRes->SetTexture("sTexture", imGuiData.fontTex);
        imGuiData.shaderConfig = imGuiData.shaderProgram->GetDefaultShaderConfig();

        /* initialize rendering resources */
        renderPipeline->SetOffScreenOutput(true);
        colorImage = renderPipeline->GetOutputColor();
        gameDepthImage = renderPipeline->GetOutputDepth();

        /* imgui render pass */
        editorPass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment colorAttachment;
        colorAttachment.format = imGuiData.editorRT->GetDescription().format;
        colorAttachment.loadOp = Gfx::AttachmentLoadOperation::Clear;
        colorAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;

        editorPass->SetAttachments({colorAttachment}, std::nullopt);
        Gfx::RenderPass::Subpass subpass;
        subpass.colors.push_back(0);
        editorPass->SetSubpass({subpass});
        frameBuffer = Gfx::GfxDriver::Instance()->CreateFrameBuffer(editorPass);
        frameBuffer->SetAttachments({imGuiData.editorRT});

        /* game depth image*/
        Gfx::ImageDescription ourGameDepthDesc = gameDepthImage->GetDescription();
        ourGameDepthDesc.format = Gfx::ImageFormat::D24_UNorm_S8_UInt;
        ourGameDepthImage = Gfx::GfxDriver::Instance()->CreateImage(ourGameDepthDesc, Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::TransferDst);

        /* scene pass data */
        scenePass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        Gfx::RenderPass::Attachment sceneDepthAttachment;
        sceneDepthAttachment.format = gameDepthImage->GetDescription().format;
        sceneDepthAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
        sceneDepthAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::RenderPass::Attachment sceneColorAttachment;
        sceneColorAttachment.format = colorImage->GetDescription().format;
        sceneColorAttachment.loadOp = Gfx::AttachmentLoadOperation::Load;
        sceneColorAttachment.storeOp = Gfx::AttachmentStoreOperation::Store;
        scenePass->SetAttachments({sceneColorAttachment}, sceneDepthAttachment);

        Gfx::RenderPass::Subpass editorPass0;
        editorPass0.colors.push_back(0);
        editorPass0.depthAttachment = 1;
        scenePass->SetSubpass({editorPass0});
        sceneFrameBuffer = Gfx::GfxDriver::Instance()->CreateFrameBuffer(scenePass);
        sceneFrameBuffer->SetAttachments({colorImage, ourGameDepthImage});

        outlineByStencil = AssetDatabase::Instance()->GetShader("Internal/OutlineByStencil");
        outlineByStencilDrawOutline = AssetDatabase::Instance()->GetShader("Internal/OutlineByStencilDrawOutline");
    }

    void GameEditor::ProcessEvent(const SDL_Event& event)
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void GameEditor::Tick()
    {
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        // ImGui::ShowDemoWindow(nullptr);

        DrawMainMenu();

        if (projectManagement->IsInitialized())
        {
            sceneTreeWindow->Tick();
            inspector->Tick();
            assetExplorer->Tick();
            gameSceneWindow->Tick(colorImage);
        }
        if (projectWindow != nullptr)
        {
            bool open = true;
            projectWindow->Tick(open);
            if (!open) projectWindow = nullptr;
        }

    }

    void GameEditor::Render()
    {
        ImGui::Render();

        auto cmdBuf = gfxDriver->CreateCommandBuffer();
        RenderSceneGUI(cmdBuf);
        RenderEditor(cmdBuf);
        cmdBuf->Blit(imGuiData.editorRT, gfxDriver->GetSwapChainImageProxy());
        gfxDriver->ExecuteCommandBuffer(std::move(cmdBuf));
    }

    void GameEditor::RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf)
    {
        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;

        GameObject* obj = dynamic_cast<GameObject*>(editorContext->currentSelected.Get());
        if (obj == nullptr) return;
        auto meshRenderer = obj->GetComponent<MeshRenderer>();
        if (meshRenderer == nullptr) return;
        auto mesh = meshRenderer->GetMesh();
        auto material = meshRenderer->GetMaterial();
        if (mesh == nullptr || material == nullptr) return;

        cmdBuf->Blit(gameDepthImage, ourGameDepthImage);
        cmdBuf->BeginRenderPass(scenePass, sceneFrameBuffer, clears);
        // draw 3D Scene GUI

        // draw outline using stencil
        if (mesh != nullptr && material != nullptr)
        {
            cmdBuf->BindVertexBuffer(mesh->GetMeshBindingInfo().bindingBuffers, mesh->GetMeshBindingInfo().bindingOffsets, 0);
            cmdBuf->BindResource(material->GetShaderResource());
            cmdBuf->BindShaderProgram(outlineByStencil->GetShaderProgram(), outlineByStencil->GetDefaultShaderConfig());
            cmdBuf->BindIndexBuffer(mesh->GetMeshBindingInfo().indexBuffer.Get(), mesh->GetMeshBindingInfo().indexBufferOffset);
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
            cmdBuf->BindShaderProgram(outlineByStencilDrawOutline->GetShaderProgram(), outlineByStencilDrawOutline->GetDefaultShaderConfig());
            cmdBuf->DrawIndexed(mesh->GetVertexDescription().index.count, 1, 0, 0, 0);
        }
        cmdBuf->EndRenderPass();

    }

    void GameEditor::RenderEditor(RefPtr<CommandBuffer> cmdBuf)
    {
        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;
        cmdBuf->BeginRenderPass(editorPass, frameBuffer, clears);

        ImDrawData* drawData = ImGui::GetDrawData();

        // update scale  and translate
        float scale[2];
        scale[0] = 2.0f / drawData->DisplaySize.x;
        scale[1] = 2.0f / drawData->DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - drawData->DisplayPos.x * scale[0];
        translate[1] = -1.0f - drawData->DisplayPos.y * scale[1];
        imGuiData.generalShaderRes->SetUniform("uPushConstant.uScale", &scale);
        imGuiData.generalShaderRes->SetUniform("uPushConstant.uTranslate", &translate);

        if (drawData->TotalVtxCount > 0)
        {
            // Create or resize the vertex/index buffers
            size_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
            size_t indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

            if (imGuiData.vertexBuffer->GetSize() < vertexSize)
                imGuiData.vertexBuffer->Resize(vertexSize);
            if (imGuiData.indexBuffer->GetSize() < indexSize)
                imGuiData.indexBuffer->Resize(indexSize);

            ImDrawVert* vtxDst = (ImDrawVert*)imGuiData.vertexBuffer->GetCPUVisibleAddress();
            ImDrawIdx* idxDst = (ImDrawIdx*)imGuiData.indexBuffer->GetCPUVisibleAddress();
            for (int n = 0; n < drawData->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = drawData->CmdLists[n];
                memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtxDst += cmd_list->VtxBuffer.Size;
                idxDst += cmd_list->IdxBuffer.Size;
            }
        }

        // draw
        // Will project scissor/clipping rectangles into framebuffer space
        int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        ImVec2 clipOff = drawData->DisplayPos;         // (0,0) unless using multi-viewports
        ImVec2 clipScale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
                                                       //
        uint32_t globalIdxOffset = 0;
        uint32_t globalVtxOffset = 0;

        cmdBuf->BindShaderProgram(imGuiData.shaderProgram, imGuiData.shaderProgram->GetDefaultShaderConfig());
        cmdBuf->BindResource(imGuiData.generalShaderRes);
        cmdBuf->BindIndexBuffer(imGuiData.indexBuffer.Get(), 0);
        // hard coded ImGui shader's vertex input
        cmdBuf->BindVertexBuffer({imGuiData.vertexBuffer}, {0}, 0);

        bool isGeneralResourceBinded = true;
        for(int i = 0 ; i < drawData->CmdListsCount; ++i)
        {
            const ImDrawList* cmdList = drawData->CmdLists[i];
            for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdI];

                // project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

                Gfx::Image* image = (Gfx::Image*)pcmd->TextureId;
                if (image != nullptr)
                {
                    auto imageRes = imGuiData.GetImageResource();
                    imageRes->SetTexture("sTexture", image);
                    cmdBuf->BindResource(imageRes);
                    isGeneralResourceBinded = false;
                }
                else if (isGeneralResourceBinded == false)
                {
                    isGeneralResourceBinded = true;
                    cmdBuf->BindResource(imGuiData.generalShaderRes);
                }

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fbWidth) { clip_max.x = (float)fbWidth; }
                if (clip_max.y > fbHeight) { clip_max.y = (float)fbHeight; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // apply scissor/clipping rectangle
                Rect2D scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                cmdBuf->SetScissor(0, 1, &scissor);

                cmdBuf->DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + globalIdxOffset, pcmd->VtxOffset + globalVtxOffset, 0);
            }
            globalIdxOffset += cmdList->IdxBuffer.Size;
            globalVtxOffset += cmdList->VtxBuffer.Size;
        }
        cmdBuf->EndRenderPass();
        imGuiData.ResetImageResource();
    }

    RefPtr<Gfx::ShaderResource> GameEditor::ImGuiData::GetImageResource()
    {
        currCacheIndex += 1;

        if (imageResourcesCache.size() <= currCacheIndex)
        {
            imageResourcesCache.push_back(Gfx::GfxDriver::Instance()->CreateShaderResource(shaderProgram, Gfx::ShaderResourceFrequency::Global));
            assert(currCacheIndex + 1 == imageResourcesCache.size());
            return imageResourcesCache.back();
        }

        return imageResourcesCache[currCacheIndex];
    }

    void GameEditor::DrawMainMenu()
    {
        ImGui::BeginMainMenuBar();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Project")) {
                projectWindow = MakeUnique<ProjectWindow>(editorContext, projectManagement);
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Create Scene") && GameSceneManager::Instance()->GetActiveGameScene() == nullptr)
        {
            auto newScene = MakeUnique<GameScene>();
            auto refNewScene = AssetDatabase::Instance()->Save(std::move(newScene), "./Assets/test.game");
            GameSceneManager::Instance()->SetActiveGameScene(refNewScene);
        }
        if (ImGui::MenuItem("Save"))
        {
            AssetDatabase::Instance()->SaveAll();
            projectManagement->Save();
        }

        if (ImGui::MenuItem("Load"))
        {

        }
        ImGui::EndMainMenuBar();
    }
}
