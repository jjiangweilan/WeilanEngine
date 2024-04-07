#include "Renderer.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/RenderGraph/NodeBuilder.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "spdlog/spdlog.h"

namespace Editor
{
static const char* imguiShader =
    R"(#version 450 core

#if CONFIG
name: ImGui
interleaved: true
blend: 
- srcAlpha oneMinusSrcAlpha
blendOp:
- add
cull: none
#endif

#if VERT
#define aColor aColor_8
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
#endif

#if FRAG
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler2D sTexture_sampler;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = In.Color * texture(sTexture_sampler, In.UV.st);
}
#endif
)";
void BuildGraphNode() {}

Renderer::~Renderer() {}

Renderer::Renderer(Gfx::Image* finalImage, Gfx::Image* fontImage)
{
    indexBuffer = GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst, 2048, false});
    vertexBuffer =
        GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst, 2048, false});
    stagingBuffer =
        GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Transfer_Src | Gfx::BufferUsage::Transfer_Dst, 4096, true});
    stagingBuffer2 =
        GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Transfer_Src | Gfx::BufferUsage::Transfer_Dst, 4096, true});

    ShaderCompiler compiler;
    compiler.Compile("", imguiShader, false);

    auto config = compiler.GetConfig();
    auto& verSPV = compiler.GetVertexSPV();
    auto& fragSPV = compiler.GetFragSPV();
    shaderProgram = GetGfxDriver()->CreateShaderProgram("ImGui", config, verSPV, fragSPV);

    this->fontImage = fontImage;
    this->finalImage = finalImage;
    graph = std::make_unique<RenderGraph::Graph>();
    BuildGraph();
}

void Renderer::BuildGraph()
{
    graph->Clear();

    auto renderEditor = graph->AddNode(
        [&](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
        { this->RenderEditor(cmd, pass, res); },
        {
            {
                .name = "output image",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write | Gfx::AccessMaskFlags::Color_Attachment_Read,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .externalImage = finalImage,
            },
        },
        {
            {
                .colors =
                    {
                        {
                            .handle = 0,
                            .loadOp = Gfx::AttachmentLoadOperation::Clear,
                            .storeOp = Gfx::AttachmentStoreOperation::Store,
                        },
                    },
            },
        }
    );

    graph->Process(renderEditor, 0);

    return;
}

void Renderer::RenderEditor(Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
{
    ImGui::Render();

    ImDrawData* imguiDrawData = ImGui::GetDrawData();

    if (imguiDrawData->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertexSize = imguiDrawData->TotalVtxCount * sizeof(ImDrawVert);
        size_t indexSize = imguiDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        size_t stagingSize = 0;
        bool createVertex = false, createIndex = false, createStaging = false;
        if (vertexBuffer->GetSize() < vertexSize)
        {
            createVertex = true;
            stagingSize = vertexSize + indexSize;
        }

        if (indexBuffer->GetSize() < indexSize)
        {
            createIndex = true;
            stagingSize = vertexSize + indexSize;
        }

        if (stagingSize != 0)
        {
            createStaging = true;
        }

        if (createVertex || createIndex || createStaging)
        {
            GetGfxDriver()->WaitForIdle();
            if (createVertex)
                vertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(
                    {Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst, vertexSize, false}
                );
            if (createIndex)
                indexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(
                    {Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst, indexSize, false}
                );
            if (createStaging)
            {
                stagingBuffer = GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Transfer_Src, stagingSize, true});
                stagingBuffer2 = GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Transfer_Src, stagingSize, true});
            }
        }

        std::swap(stagingBuffer, stagingBuffer2);

        ImDrawVert* vtxDst = (ImDrawVert*)stagingBuffer->GetCPUVisibleAddress();
        ImDrawIdx* idxDst = (ImDrawIdx*)(((uint8_t*)stagingBuffer->GetCPUVisibleAddress()) + vertexSize);
        for (int n = 0; n < imguiDrawData->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = imguiDrawData->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }

        Gfx::BufferCopyRegion vertexCopy[1];
        Gfx::BufferCopyRegion indexCopy[1];
        vertexCopy[0] = {0, 0, vertexSize};
        indexCopy[0] = {vertexSize, 0, indexSize};
        cmd.CopyBuffer(stagingBuffer, vertexBuffer, vertexCopy);
        cmd.CopyBuffer(stagingBuffer, indexBuffer, indexCopy);
    }

    Gfx::Image* color = (Gfx::Image*)res.at(0)->GetResource();
    uint32_t width = color->GetDescription().width;
    uint32_t height = color->GetDescription().height;
    cmd.SetViewport({.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1});
    std::vector<Gfx::ClearValue> clears(2);
    clears[0].color = {{0, 0, 0, 0}};
    clears[1].depthStencil.depth = 1;
    cmd.BeginRenderPass(pass, clears);

    // draw
    // Will project scissor/clipping rectangles into framebuffer space
    int fbWidth = (int)(imguiDrawData->DisplaySize.x * imguiDrawData->FramebufferScale.x);
    int fbHeight = (int)(imguiDrawData->DisplaySize.y * imguiDrawData->FramebufferScale.y);
    ImVec2 clipOff = imguiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clipScale = imguiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
                                                        //
    uint32_t globalIdxOffset = 0;
    uint32_t globalVtxOffset = 0;

    cmd.BindShaderProgram(shaderProgram.get(), shaderProgram->GetDefaultShaderConfig());
    cmd.BindIndexBuffer(indexBuffer.get(), 0, Gfx::IndexBufferType::UInt16);
    Gfx::DescriptorBinding bindings[] = {{
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .imageView = &fontImage->GetDefaultImageView(),
    }};
    cmd.PushDescriptor(*shaderProgram, 0, bindings);

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
    cmd.SetPushConstant(shaderProgram.get(), &scale2Translate2);

    // hard coded ImGui shader's vertex input
    // cmdBuf->BindVertexBuffer({imGuiData.vertexBuffer}, {0}, 0);
    Gfx::VertexBufferBinding vertexBinding[] = {{vertexBuffer.get(), 0}};
    cmd.BindVertexBuffer(vertexBinding, 0);

    bool isGeneralResourceBinded = true;
    for (int i = 0; i < imguiDrawData->CmdListsCount; ++i)
    {
        const ImDrawList* cmdList = imguiDrawData->CmdLists[i];
        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++)
        {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdI];

            // project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
            ImVec2 clip_max((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

            Gfx::ImageView* imageView = (Gfx::ImageView*)pcmd->TextureId;
            if (imageView != nullptr)
            {
                Gfx::DescriptorBinding bindings[] = {{
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .imageView = imageView,
                }};
                cmd.PushDescriptor(*shaderProgram, 0, bindings);
                isGeneralResourceBinded = false;
            }
            else if (!isGeneralResourceBinded)
            {
                Gfx::DescriptorBinding bindings[] = {{
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .imageView = &fontImage->GetDefaultImageView(),
                }};
                cmd.PushDescriptor(*shaderProgram, 0, bindings);
                isGeneralResourceBinded = true;
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
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            // apply scissor/clipping rectangle
            Rect2D scissor;
            scissor.offset.x = (int32_t)(clip_min.x);
            scissor.offset.y = (int32_t)(clip_min.y);
            scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
            scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
            cmd.SetScissor(0, 1, &scissor);
            cmd.DrawIndexed(
                pcmd->ElemCount,
                1,
                pcmd->IdxOffset + globalIdxOffset,
                pcmd->VtxOffset + globalVtxOffset,
                0
            );
        }
        globalIdxOffset += cmdList->IdxBuffer.Size;
        globalVtxOffset += cmdList->VtxBuffer.Size;
    }
    cmd.EndRenderPass();
}

void Renderer::Execute(Gfx::CommandBuffer& cmd)
{
    graph->Execute(cmd);
}
} // namespace Editor
