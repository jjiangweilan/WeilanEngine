#include "GameEditorRenderer.hpp"
#include <vector>
namespace Engine::Editor
{

GameEditorRenderer::GameEditorRenderer()
{
    imGuiData.indexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Index, 1024, true});
    imGuiData.vertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Vertex, 1024, true});
    imGuiData.shaderProgram = AssetDatabase::Instance()->GetShader("ImGui")->GetShaderProgram();
    imGuiData.generalShaderRes =
        Gfx::GfxDriver::Instance()->CreateShaderResource(imGuiData.shaderProgram, Gfx::ShaderResourceFrequency::Global);

    // imGuiData.editorRT creation
    {
        Gfx::ImageDescription editorRTDesc;
        editorRTDesc.width = GetGfxDriver()->GetWindowSize().width;
        editorRTDesc.height = GetGfxDriver()->GetWindowSize().height;
        editorRTDesc.format = Gfx::ImageFormat::R8G8B8A8_UNorm;
        imGuiData.editorRT = Gfx::GfxDriver::Instance()->CreateImage(
            editorRTDesc,
            Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture);
        imGuiData.editorRT->SetName("Editor RT");
    }

    // imGuiData.fontTex creation
    Gfx::ImageDescription fontTexDesc;
    int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontTexDesc.data, &width, &height);
    fontTexDesc.width = width;
    fontTexDesc.height = height;
    fontTexDesc.format = Gfx::ImageFormat::R8G8B8A8_UNorm;
    imGuiData.fontTex = Gfx::GfxDriver::Instance()->CreateImage(fontTexDesc, Gfx::ImageUsage::Texture);
    imGuiData.generalShaderRes->SetTexture("sTexture", imGuiData.fontTex);
    imGuiData.shaderConfig = imGuiData.shaderProgram->GetDefaultShaderConfig();

    assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload(
        [this](RefPtr<AssetObject> obj)
        {
            Shader* casted = dynamic_cast<Shader*>(obj.Get());
            if (casted && casted->GetName() == "ImGui")
            {
                this->imGuiData.shaderProgram = casted->GetShaderProgram();
                this->imGuiData.generalShaderRes =
                    Gfx::GfxDriver::Instance()->CreateShaderResource(imGuiData.shaderProgram,
                                                                     Gfx::ShaderResourceFrequency::Global);
                this->imGuiData.generalShaderRes->SetTexture("sTexture", this->imGuiData.fontTex);
                this->imGuiData.shaderConfig = imGuiData.shaderProgram->GetDefaultShaderConfig();
                this->imGuiData.ClearImageResource();
            }

            if (casted && casted->GetName() == "Internal/SimpleBlend")
            {
                // res = Gfx::GfxDriver::Instance()->CreateShaderResource(casted->GetShaderProgram(),
                //     Gfx::ShaderResourceFrequency::Shader);
            }
        });

    gameEditorPass = renderGraph.AddNode<RGraph::RenderPassNode>();
}

RefPtr<Gfx::ShaderResource> GameEditorRenderer::ImGuiData::GetImageResource()
{
    currCacheIndex += 1;

    if (imageResourcesCache.size() <= currCacheIndex)
    {
        imageResourcesCache.push_back(
            Gfx::GfxDriver::Instance()->CreateShaderResource(shaderProgram, Gfx::ShaderResourceFrequency::Global));
        assert(currCacheIndex + 1 == imageResourcesCache.size());
        return imageResourcesCache.back();
    }

    return imageResourcesCache[currCacheIndex];
}

void GameEditorRenderer::ImGuiData::ClearImageResource() { imageResourcesCache.clear(); }

void GameEditorRenderer::Render(CommandBuffer* cmdBuf, RGraph::ResourceStateTrack& stateTrack)
{
    auto& drawList = gameEditorPass->GetDrawList();
    drawList.clear();

    std::vector<Gfx::ClearValue> clears(2);
    clears[0].color = {{0, 0, 0, 0}};
    clears[1].depthStencil.depth = 1;
    // cmdBuf->BeginRenderPass(editorPass, clears);

    ImDrawData* imguiDrawData = ImGui::GetDrawData();

    if (imguiDrawData->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertexSize = imguiDrawData->TotalVtxCount * sizeof(ImDrawVert);
        size_t indexSize = imguiDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        if (imGuiData.vertexBuffer->GetSize() < vertexSize)
            assert(0 && "Not implemented"); // imGuiData.vertexBuffer->Resize(vertexSize);
        if (imGuiData.indexBuffer->GetSize() < indexSize)
            assert(0 && "Not implemented"); // imGuiData.vertexBuffer->Resize(vertexSize);

        ImDrawVert* vtxDst = (ImDrawVert*)imGuiData.vertexBuffer->GetCPUVisibleAddress();
        ImDrawIdx* idxDst = (ImDrawIdx*)imGuiData.indexBuffer->GetCPUVisibleAddress();
        for (int n = 0; n < imguiDrawData->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = imguiDrawData->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }
    }

    // draw
    // Will project scissor/clipping rectangles into framebuffer space
    int fbWidth = (int)(imguiDrawData->DisplaySize.x * imguiDrawData->FramebufferScale.x);
    int fbHeight = (int)(imguiDrawData->DisplaySize.y * imguiDrawData->FramebufferScale.y);
    ImVec2 clipOff = imguiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clipScale = imguiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
                                                        //
    uint32_t globalIdxOffset = 0;
    uint32_t globalVtxOffset = 0;

    RGraph::DrawData drawData;
    drawData.shader = imGuiData.shaderProgram.Get();
    drawData.shaderResource = imGuiData.generalShaderRes.Get();
    drawData.indexBuffer = RGraph::IndexBuffer{imGuiData.indexBuffer.Get(), 0, IndexBufferType::UInt16};

    // cmdBuf->BindShaderProgram(imGuiData.shaderProgram, imGuiData.shaderProgram->GetDefaultShaderConfig());
    // cmdBuf->BindResource(imGuiData.generalShaderRes);
    // cmdBuf->BindIndexBuffer(imGuiData.indexBuffer.Get(), 0, IndexBufferType::UInt16);
    //
    // update scale  and translate
    float scale2Translate2[4];
    scale2Translate2[0] = 2.0f / imguiDrawData->DisplaySize.x;
    scale2Translate2[1] = 2.0f / imguiDrawData->DisplaySize.y;
    scale2Translate2[2] = -1.0f - imguiDrawData->DisplayPos.x * scale2Translate2[0];
    scale2Translate2[3] = -1.0f - imguiDrawData->DisplayPos.y * scale2Translate2[1];
    // cmdBuf->SetPushConstant(imGuiData.shaderProgram, &scale2Translate2);
    drawData.pushConstant = RGraph::PushConstant{imGuiData.shaderProgram.Get(), glm::mat4(0), glm::mat4(0)};
    drawData.pushConstant->mat0[0][0] = scale2Translate2[0];
    drawData.pushConstant->mat0[0][1] = scale2Translate2[1];
    drawData.pushConstant->mat0[0][2] = scale2Translate2[2];
    drawData.pushConstant->mat0[0][3] = scale2Translate2[3];

    // hard coded ImGui shader's vertex input
    // cmdBuf->BindVertexBuffer({imGuiData.vertexBuffer}, {0}, 0);
    drawData.vertexBuffer = {{imGuiData.vertexBuffer.Get(), 0}};
    drawList.push_back(drawData);

    bool isGeneralResourceBinded = true;
    for (int i = 0; i < imguiDrawData->CmdListsCount; ++i)
    {
        const ImDrawList* cmdList = imguiDrawData->CmdLists[i];
        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++)
        {
            RGraph::DrawData drawData;
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdI];

            // project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
            ImVec2 clip_max((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

            Gfx::Image* image = (Gfx::Image*)pcmd->TextureId;
            if (image != nullptr)
            {
                auto imageRes = imGuiData.GetImageResource();
                imageRes->SetTexture("sTexture", image);
                // cmdBuf->BindResource(imageRes);
                drawData.shaderResource = imageRes.Get();
                isGeneralResourceBinded = false;
            }
            else if (isGeneralResourceBinded == false)
            {
                isGeneralResourceBinded = true;
                // cmdBuf->BindResource(imGuiData.generalShaderRes);
                drawData.shaderResource = imGuiData.generalShaderRes.Get();
            }

            // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
            if (clip_min.x < 0.0f)
            {
                clip_min.x = 0.0f;
            }
            if (clip_min.y < 0.0f)
            {
                clip_min.y = 0.0f;
            }
            if (clip_max.x > fbWidth)
            {
                clip_max.x = (float)fbWidth;
            }
            if (clip_max.y > fbHeight)
            {
                clip_max.y = (float)fbHeight;
            }
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) continue;

            // apply scissor/clipping rectangle
            Rect2D scissor;
            scissor.offset.x = (int32_t)(clip_min.x);
            scissor.offset.y = (int32_t)(clip_min.y);
            scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
            scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
            // cmdBuf->SetScissor(0, 1, &scissor);
            drawData.scissor = scissor;

            drawData.drawIndexed = RGraph::DrawIndex{pcmd->ElemCount,
                                                     1,
                                                     pcmd->IdxOffset + globalIdxOffset,
                                                     pcmd->VtxOffset + globalVtxOffset,
                                                     0};
            // cmdBuf->DrawIndexed(pcmd->ElemCount,
            //                                      1,
            //                                      pcmd->IdxOffset + globalIdxOffset,
            //                                      pcmd->VtxOffset + globalVtxOffset,
            //                                      0);
            //
            drawList.push_back(drawData);
        }
        globalIdxOffset += cmdList->IdxBuffer.Size;
        globalVtxOffset += cmdList->VtxBuffer.Size;
    }
    // cmdBuf->EndRenderPass();
    imGuiData.ResetImageResource();
    renderGraph.Execute(cmdBuf, stateTrack);
}
} // namespace Engine::Editor
