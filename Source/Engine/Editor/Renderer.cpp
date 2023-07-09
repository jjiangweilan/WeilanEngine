#include "Renderer.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "Rendering/RenderGraph/NodeBuilder.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{

const char* imguiShader =
    R"(
#version 450 core

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
layout(set=0, binding=0) uniform sampler2D sTexture_sampler_linear;

layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
layout(location = 0) out vec4 fColor;
void main()
{
    fColor = In.Color * texture(sTexture_sampler_linear, In.UV.st);
}
#endif)";

void BuildGraphNode() {}

std::unique_ptr<Gfx::Image> CreateImGuiFont(const char* customFont);

Renderer::~Renderer() {}

Renderer::Renderer(const char* customFont)
{
    indexBuffer = GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Index, 1024, true});
    vertexBuffer = GetGfxDriver()->CreateBuffer({Gfx::BufferUsage::Vertex, 1024, true});

    ShaderCompiler compiler;
    compiler.Compile(imguiShader, false);

    auto& config = compiler.GetConfig();
    auto& verSPV = compiler.GetVertexSPV();
    auto& fragSPV = compiler.GetFragSPV();
    shaderProgram = GetGfxDriver()->CreateShaderProgram("ImGui", &config, verSPV, fragSPV);

    fontImage = CreateImGuiFont(customFont);
}

std::tuple<RenderGraph::RenderNode*, RenderGraph::ResourceHandle> Renderer::BuildGraph(
    RenderGraph::Graph& graph,
    RenderGraph::RenderNode* colorNode,
    RenderGraph::ResourceHandle colorHandle,
    RenderGraph::RenderNode* depthNode,
    RenderGraph::ResourceHandle depthHandle
)
{
    std::vector<RenderGraph::NodeBuilder::BlitDescription> blitDescs;
    if (colorNode)
    {
        colorNode->GetPass()->GetResourceRef(colorHandle);
        auto& colorDesc = colorNode->GetPass()->GetResourceDescription(colorHandle);

        if (colorDesc.type != RenderGraph::ResourceType::Image)
        {
            throw std::logic_error("color node has to be Image type");
        }
        blitDescs.push_back({
            .srcNode = *colorNode,
            .srcHandle = colorHandle,
            .dstCreateInfo = colorDesc.imageCreateInfo,
            .dstHandle = 0,
        });
    }

    if (depthNode)
    {
        auto& depthDesc = depthNode->GetPass()->GetResourceDescription(depthHandle);
        if (depthDesc.type != RenderGraph::ResourceType::Image)
        {
            throw std::logic_error("depth node has to be Image type");
        }

        blitDescs.push_back({
            .srcNode = *depthNode,
            .srcHandle = depthHandle,
            .dstCreateInfo = depthDesc.imageCreateInfo,
            .dstHandle = 1,
        });
    }
    auto blitBuildResult = RenderGraph::NodeBuilder::Blit(blitDescs);
    auto sceneCopy = graph.AddNode(blitBuildResult.execFunc, blitBuildResult.resourceDescs, blitBuildResult.subpass);

    auto renderEditor = graph.AddNode(
        [&](auto& cmd, auto& pass, auto& res) { this->RenderEditor(cmd, pass, res); },
        {
            {
                .name = "SwapChain",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write | Gfx::AccessMaskFlags::Color_Attachment_Read,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .externalImage = GetGfxDriver()->GetSwapChainImageProxy().Get(),
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

    return {renderEditor, 0};
}

void Renderer::RenderEditor(Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
{
    ImGui::Render();

    std::vector<Gfx::ClearValue> clears(2);
    clears[0].color = {{0, 0, 0, 0}};
    clears[1].depthStencil.depth = 1;
    cmd.BeginRenderPass(&pass, clears);

    ImDrawData* imguiDrawData = ImGui::GetDrawData();

    if (imguiDrawData->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertexSize = imguiDrawData->TotalVtxCount * sizeof(ImDrawVert);
        size_t indexSize = imguiDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        if (vertexBuffer->GetSize() < vertexSize)
        {
            GetGfxDriver()->WaitForIdle();
            vertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Vertex, vertexSize, true});
        }
        if (indexBuffer->GetSize() < indexSize)
        {
            GetGfxDriver()->WaitForIdle();
            indexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer({Gfx::BufferUsage::Index, indexSize, true});
        }

        ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer->GetCPUVisibleAddress();
        ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer->GetCPUVisibleAddress();
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

    cmd.BindShaderProgram(shaderProgram.get(), shaderProgram->GetDefaultShaderConfig());
    cmd.BindIndexBuffer(indexBuffer.get(), 0, Gfx::IndexBufferType::UInt16);
    Gfx::DescriptorBinding bindings[] = {{
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .image = fontImage.get(),
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

            Gfx::Image* image = (Gfx::Image*)pcmd->TextureId;
            if (image != nullptr)
            {
                Gfx::DescriptorBinding bindings[] = {{
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .image = image,
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
                    .image = fontImage.get(),
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

std::unique_ptr<Gfx::Image> CreateImGuiFont(const char* customFont)
{
    assert(customFont == nullptr && "customFont not implemented");

    unsigned char* fontData;
    auto& io = ImGui::GetIO();
    ImFontConfig config;
    ImFont* font = nullptr;
    // if (customFont)
    // {
    //     static const ImWchar icon_ranges[] = {0x0020, 0xffff, 0};
    //     font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
    //         (std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources" / "Cousine Regular Nerd Font Complete.ttf")
    //             .string()
    //             .c_str(),
    //         14,
    //         &config,
    //         icon_ranges
    //     );
    // }
    io.FontDefault = font;
    int width, height, bytePerPixel;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontData, &width, &height, &bytePerPixel);
    auto fontImage = GetGfxDriver()->CreateImage(
        {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
            .isCubemap = false,
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    uint32_t fontTexSize = bytePerPixel * width * height;
    auto tsfBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Src,
        .size = fontTexSize,
        .visibleInCPU = true,
    });
    memcpy(tsfBuffer->GetCPUVisibleAddress(), fontData, fontTexSize);

    ImmediateGfx::OnetimeSubmit(
        [&tsfBuffer, &fontImage, width, height](Gfx::CommandBuffer& cmd)
        {
            Gfx::BufferImageCopyRegion copyRegion[] = {{
                .srcOffset = 0,
                .layers =
                    {
                        .aspectMask = Gfx::ImageAspectFlags::Color,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .offset = {0, 0, 0},
                .extend = {(uint32_t)width, (uint32_t)height, 1},
            }};
            Gfx::GPUBarrier toTransferDst = {
                .image = fontImage,
                .srcStageMask = Gfx::PipelineStage::Top_Of_Pipe,
                .dstStageMask = Gfx::PipelineStage::Transfer,
                .srcAccessMask = Gfx::AccessMask::None,
                .dstAccessMask = Gfx::AccessMask::Transfer_Write,
                .imageInfo = {
                    .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .oldLayout = Gfx::ImageLayout::Undefined,
                    .newLayout = Gfx::ImageLayout::Transfer_Dst,
                    .subresourceRange =
                        {
                            .aspectMask = Gfx::ImageAspectFlags::Color,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                }};
            cmd.Barrier(&toTransferDst, 1);
            cmd.CopyBufferToImage(tsfBuffer, fontImage, copyRegion);

            Gfx::GPUBarrier toTexture = {
                .image = fontImage,
                .srcStageMask = Gfx::PipelineStage::Transfer,
                .dstStageMask = Gfx::PipelineStage::Bottom_Of_Pipe,
                .srcAccessMask = Gfx::AccessMask::Transfer_Write,
                .dstAccessMask = Gfx::AccessMask::None,
                .imageInfo = {
                    .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .oldLayout = Gfx::ImageLayout::Transfer_Dst,
                    .newLayout = Gfx::ImageLayout::Shader_Read_Only,
                    .subresourceRange =
                        {
                            .aspectMask = Gfx::ImageAspectFlags::Color,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                }};
            cmd.Barrier(&toTexture, 1);
        }
    );
    return fontImage;
}
} // namespace Engine::Editor
