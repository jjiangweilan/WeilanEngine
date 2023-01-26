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

    // imGuiData.fontTex creation
    Gfx::ImageDescription fontTexDesc;
    unsigned char* fontData;
    auto& io = ImGui::GetIO();
    ImFontConfig config;
    static const ImWchar icon_ranges[] = {0x0020, 0xffff, 0};
    ImFont* font =
        ImGui::GetIO().Fonts->AddFontFromFileTTF("Cousine Regular Nerd Font Complete.ttf", 14, &config, icon_ranges);
    io.FontDefault = font;
    int width, height, bytePerPixel;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontData, &width, &height, &bytePerPixel);
    int fontTexSize = bytePerPixel * width * height;
    fontTexDesc.data = new unsigned char[fontTexSize];
    memcpy(fontTexDesc.data, fontData, fontTexSize);
    fontTexDesc.width = width;
    fontTexDesc.height = height;
    fontTexDesc.format = Gfx::ImageFormat::R8G8B8A8_UNorm;
    imGuiData.fontTex = MakeUnique<Texture>(fontTexDesc);
    imGuiData.generalShaderRes->SetTexture("sTexture", imGuiData.fontTex->GetGfxImage());
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
                this->imGuiData.generalShaderRes->SetTexture("sTexture", this->imGuiData.fontTex->GetGfxImage());
                this->imGuiData.shaderConfig = imGuiData.shaderProgram->GetDefaultShaderConfig();
                this->imGuiData.ClearImageResource();
            }

            if (casted && casted->GetName() == "Internal/SimpleBlend")
            {
                // res = Gfx::GfxDriver::Instance()->CreateShaderResource(casted->GetShaderProgram(),
                //     Gfx::ShaderResourceFrequency::Shader);
            }
        });
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

void GameEditorRenderer::Tick() {}

void GameEditorRenderer::ImGuiData::ClearImageResource() { imageResourcesCache.clear(); }

void GameEditorRenderer::ResizeWindow(Extent2D windowSize)
{
    gameEditorColorNode->width = windowSize.width;
    gameEditorColorNode->height = windowSize.height;
    gameEditorDepthNode->width = windowSize.width;
    gameEditorDepthNode->height = windowSize.height;
}

RGraph::Port* GameEditorRenderer::BuildGraph(RGraph::RenderGraph* graph,
                                             RGraph::Port* finalColorPort,
                                             RGraph::Port* finalDepthPor,
                                             Extent2D windowSize)
{
    auto gameSceneImageNode = graph->AddNode<RGraph::ImageNode>();
    gameSceneImageNode->SetName("GameEditorRenderer-GameSceneImage");
    gameSceneImageNode->SetExternalImage(gameSceneImageTarget);
    gameSceneImageNode->initialState = {Gfx::PipelineStage::Bottom_Of_Pipe,
                                        Gfx::AccessMask::None,
                                        Gfx::ImageLayout::Undefined,
                                        GFX_QUEUE_FAMILY_IGNORED,
                                        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst};

    auto gameColorBlitNode = graph->AddNode<RGraph::BlitNode>();
    gameColorBlitNode->SetName("GameEditorRenderer-GameColorToColorTarget");
    gameColorBlitNode->GetPortSrcImageIn()->Connect(finalColorPort);
    gameColorBlitNode->GetPortDstImageIn()->Connect(gameSceneImageNode->GetPortOutput());

    gameEditorColorNode = graph->AddNode<RGraph::ImageNode>();
    gameEditorColorNode->width = windowSize.width;
    gameEditorColorNode->height = windowSize.height;
    gameEditorColorNode->mipLevels = 1;
    gameEditorColorNode->multiSampling = Gfx::MultiSampling::Sample_Count_1;
    gameEditorColorNode->format = Gfx::ImageFormat::R16G16B16A16_SFloat;

    gameEditorDepthNode = graph->AddNode<RGraph::ImageNode>();
    gameEditorDepthNode->width = windowSize.width;
    gameEditorDepthNode->height = windowSize.height;
    gameEditorDepthNode->mipLevels = 1;
    gameEditorDepthNode->multiSampling = Gfx::MultiSampling::Sample_Count_1;
    gameEditorDepthNode->format = Gfx::ImageFormat::D24_UNorm_S8_UInt;

    gameEditorPassNode = graph->AddNode<RGraph::RenderPassNode>();
    gameEditorPassNode->GetColorAttachmentOps()[0].loadOp = Gfx::AttachmentLoadOperation::Clear;
    gameEditorPassNode->GetClearValues()[0].color = {{0, 0, 0, 0}};
    gameEditorPassNode->GetClearValues().back().depthStencil = {1, 0};
    gameEditorPassNode->GetPortColorIn(0)->Connect(gameEditorColorNode->GetPortOutput());
    gameEditorPassNode->GetPortDepthIn()->Connect(gameEditorDepthNode->GetPortOutput());
    gameEditorPassNode->GetPortDependentAttachmentsIn()->Connect(gameColorBlitNode->GetPortDstImageOut());
    gameEditorPassNode->SetDrawList(drawList);
    return gameEditorPassNode->GetPortColorOut(0);
}

void GameEditorRenderer::Render()
{
    ImGui::Render();

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
        {
            GetGfxDriver()->WaitForIdle();
            imGuiData.vertexBuffer =
                Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Vertex, vertexSize, true});
        }
        if (imGuiData.indexBuffer->GetSize() < indexSize)
        {
            GetGfxDriver()->WaitForIdle();
            imGuiData.indexBuffer =
                Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Index, indexSize, true});
        }

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
}
} // namespace Engine::Editor
