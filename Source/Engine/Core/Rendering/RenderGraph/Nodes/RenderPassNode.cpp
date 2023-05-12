#include "RenderPassNode.hpp"
#include "Core/GameObject.hpp"
#include <format>
#include <spdlog/spdlog.h>
namespace Engine::RGraph
{
RenderPassNode::RenderPassNode()
{
    SetColorCount(1);
    depthPortIn = AddPort("Depth",
                          Port::Type::Input,
                          typeid(Gfx::Image),
                          false,
                          [](Node* bNode, Port* other, auto type)
                          {
                              RenderPassNode* node = (RenderPassNode*)bNode;
                              if (type == Port::ConnectionType::Connect)
                              {
                                  node->depthPortOut->SetResource(node->depthPortIn->GetResource());
                              }
                              else
                                  node->depthPortOut->RemoveResource(node->depthPortIn->GetResource());
                          });
    depthPortOut = AddPort("Depth", Port::Type::Output, typeid(Gfx::Image));
    dependentAttachmentIn = AddPort("dependent attachment", Port::Type::Input, typeid(Gfx::Image), true);
}

bool RenderPassNode::Preprocess(ResourceStateTrack& stateTrack)
{
    for (auto c : colorPortsIn)
    {
        if (c && c->GetResource() != nullptr)
        {
            stateTrack.GetState(c->GetResource()).imageUsages |= Gfx::ImageUsage::ColorAttachment;
        }
    }

    if (auto depthSourcePort = depthPortIn->GetConnectedPort())
    {
        auto& state = stateTrack.GetState(depthSourcePort->GetResource());
        state.imageUsages |= Gfx::ImageUsage::DepthStencilAttachment;
    }

    auto resRefs = dependentAttachmentIn->GetResources();
    for (auto res : resRefs)
    {
        auto& state = stateTrack.GetState(res);
        state.imageUsages |= Gfx::ImageUsage::Texture;
    }

    return true;
};

bool RenderPassNode::Compile(ResourceStateTrack& stateTrack)
{
    renderPass = GetGfxDriver()->CreateRenderPass();
    int colorCount = 0;
    int hasDepth = 0;

    for (auto& c : colorPortsIn)
    {
        if (c->GetResource())
            colorCount += 1;
    }
    if (depthPortIn->GetResource())
        hasDepth = 1;

    // at least one attachment is needed
    if (colorCount + hasDepth == 0)
        return false;

    colorAttachments.resize(colorCount);
    assert(colorAttachmentOps.size() == colorCount); // this is required for the following code path

    // fill attachment data
    int attachmentIndex = 0;
    int index = 0;
    for (auto& c : colorPortsIn)
    {
        if (c->GetResource())
        {
            Gfx::Image* colorImage = (Gfx::Image*)c->GetResourceVal();
            Gfx::RenderPass::Attachment& a = colorAttachments[attachmentIndex];
            a.image = colorImage;
            colorAttachmentOps[attachmentIndex].FillAttachment(a);
        }
        colorPortsOut[index]->SetResource(c->GetResource());
        index += 1;
    }

    if (hasDepth)
    {
        depthAttachment = Gfx::RenderPass::Attachment();
        depthAttachment->image = (Gfx::Image*)depthPortIn->GetResourceVal();
        depthAttachmentOp.FillAttachment(*depthAttachment);
        depthPortOut->SetResource(depthPortIn->GetResource());
    }
    else
        depthAttachment = std::nullopt;

    renderPass->AddSubpass(colorAttachments, depthAttachment);

    return true;
}

bool RenderPassNode::Execute(CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack)
{
    Port* colorPort0 = colorPortsIn.empty() ? nullptr : colorPortsIn.front();
    Port* colorConnectedPort = colorPort0 ? colorPort0->GetConnectedPort() : colorPort0;
    Port* depthConnectedPort = depthPortIn->GetConnectedPort();
    ResourceRef* color0Res = colorConnectedPort ? colorConnectedPort->GetResource() : nullptr;
    ResourceRef* depthRes = depthConnectedPort ? depthConnectedPort->GetResource() : nullptr;

    ResourceRef* attaRes = color0Res ? color0Res : depthRes;
    Gfx::Image* atta = (Gfx::Image*)attaRes->GetVal();
    Viewport viewport{.x = 0,
                      .y = 0,
                      .width = static_cast<float>(atta->GetDescription().width),
                      .height = static_cast<float>(atta->GetDescription().height),
                      .minDepth = 0,
                      .maxDepth = 1};

    cmdBuf->SetViewport(viewport);

    std::vector<GPUBarrier> barriers;

    if (color0Res)
    {
        InsertImageBarrierIfNeeded(stateTrack,
                                   color0Res,
                                   barriers,
                                   Gfx::ImageLayout::Color_Attachment,
                                   Gfx::PipelineStage::Color_Attachment_Output,
                                   Gfx::AccessMask::Color_Attachment_Read | Gfx::AccessMask::Color_Attachment_Write);
    }

    if (depthRes)
    {
        InsertImageBarrierIfNeeded(stateTrack,
                                   depthRes,
                                   barriers,
                                   Gfx::ImageLayout::Depth_Stencil_Attachment,
                                   Gfx::PipelineStage::Early_Fragment_Tests | Gfx::PipelineStage::Late_Fragment_Tests,
                                   Gfx::AccessMask::Depth_Stencil_Attachment_Read |
                                       Gfx::AccessMask::Depth_Stencil_Attachment_Write);
    }

    for (auto dep : dependentAttachmentIn->GetResources())
    {
        InsertImageBarrierIfNeeded(stateTrack,
                                   dep,
                                   barriers,
                                   Gfx::ImageLayout::Shader_Read_Only,
                                   Gfx::PipelineStage::Fragment_Shader,
                                   Gfx::AccessMask::Shader_Read);
    }

    if (!barriers.empty())
    {

        cmdBuf->Barrier(barriers.data(), barriers.size());
    }
    cmdBuf->BeginRenderPass(renderPass, clearValues);

    if (drawDataOverride.shader)
    {
        cmdBuf->BindShaderProgram(drawDataOverride.shader,
                                  drawDataOverride.shaderConfig ? *drawDataOverride.shaderConfig
                                                                : drawDataOverride.shader->GetDefaultShaderConfig());
    }

    if (drawDataOverride.shaderResource)
    {
        cmdBuf->BindResource(drawDataOverride.shaderResource);
    }

    if (drawDataOverride.indexBuffer)
    {
        cmdBuf->BindIndexBuffer(drawDataOverride.indexBuffer->buffer,
                                drawDataOverride.indexBuffer->offset,
                                drawDataOverride.indexBuffer->bufferType);
    }

    if (drawDataOverride.vertexBuffer)
    {
        cmdBuf->BindVertexBuffer(drawDataOverride.vertexBuffer.value(), 0);
    }

    if (drawDataOverride.pushConstant)
    {
        cmdBuf->SetPushConstant(drawDataOverride.pushConstant->shaderProgram, &drawDataOverride.pushConstant->mat0);
    }

    if (drawDataOverride.scissor)
    {
        cmdBuf->SetScissor(0, 1, &*drawDataOverride.scissor);
    }
    else
    {
        Rect2D rect{{0, 0}, {atta->GetDescription().width, atta->GetDescription().height}};
        cmdBuf->SetScissor(0, 1, &rect);
    }

    if (drawDataOverride.drawIndexed)
    {
        cmdBuf->DrawIndexed(drawDataOverride.drawIndexed->elementCount,
                            drawDataOverride.drawIndexed->instanceCount,
                            drawDataOverride.drawIndexed->firstIndex,
                            drawDataOverride.drawIndexed->vertexOffset,
                            drawDataOverride.drawIndexed->firstInstance);
    }

    if (drawList)
    {
        for (auto& drawData : *drawList)
        {
            if (drawData.shader && !drawDataOverride.shader)
            {
                cmdBuf->BindShaderProgram(drawData.shader,
                                          drawData.shaderConfig ? *drawData.shaderConfig
                                                                : drawData.shader->GetDefaultShaderConfig());
            }

            if (drawData.shaderResource)
            {
                if (!drawDataOverride.shaderResource)
                {
                    cmdBuf->BindResource(drawData.shaderResource);
                }
            }

            if (drawData.indexBuffer && !drawDataOverride.indexBuffer)
            {
                cmdBuf->BindIndexBuffer(drawData.indexBuffer->buffer,
                                        drawData.indexBuffer->offset,
                                        drawData.indexBuffer->bufferType);
            }

            if (drawData.vertexBuffer && !drawDataOverride.vertexBuffer)
            {
                cmdBuf->BindVertexBuffer(drawData.vertexBuffer.value(), 0);
            }

            if (drawData.pushConstant && !drawDataOverride.pushConstant)
            {
                cmdBuf->SetPushConstant(drawData.pushConstant->shaderProgram, &drawData.pushConstant->mat0);
            }

            if (drawData.scissor && !drawDataOverride.scissor)
            {
                cmdBuf->SetScissor(0, 1, &*drawData.scissor);
            }

            if (drawData.drawIndexed && !drawDataOverride.drawIndexed)
            {
                cmdBuf->DrawIndexed(drawData.drawIndexed->elementCount,
                                    drawData.drawIndexed->instanceCount,
                                    drawData.drawIndexed->firstIndex,
                                    drawData.drawIndexed->vertexOffset,
                                    drawData.drawIndexed->firstInstance);
            }
        }
    }

    cmdBuf->EndRenderPass();

    return true;
}

void RenderPassNode::SetColorCount(uint32_t count)
{
    uint32_t newCount = count;
    uint32_t oldCount = colorPortsIn.size();
    if (newCount > 8)
        return;
    if (newCount > oldCount)
    {
        for (int i = oldCount; i < newCount; ++i)
        {
            colorPortsIn.push_back(
                AddPort(fmt::format("Color {}", i).c_str(),
                        Port::Type::Input,
                        typeid(Gfx::Image),
                        false,
                        [](Node* bNode, Port* other, auto type)
                        {
                            RenderPassNode* node = (RenderPassNode*)bNode;
                            if (type == Port::ConnectionType::Connect)
                            {
                                node->colorPortsOut.back()->SetResource(node->colorPortsIn.back()->GetResource());
                            }
                            else
                                node->colorPortsOut.back()->RemoveResource(node->colorPortsIn.back()->GetResource());
                        }));
            colorPortsOut.push_back(AddPort("Color", Port::Type::Output, typeid(Gfx::Image)));
        }
    }
    else if (newCount < oldCount)
    {
        for (int i = oldCount; i > newCount; --i)
        {
            if (!colorPortsIn.empty())
            {
                auto port = colorPortsIn.back();
                RemovePort(port);
                colorPortsIn.pop_back();
            }

            if (!colorPortsOut.empty())
            {
                auto port = colorPortsOut.empty() ? nullptr : colorPortsOut.back();
                RemovePort(port);
                colorPortsOut.pop_back();
            }
        }
    }

    clearValues.resize(colorPortsIn.size() + 1);
    colorAttachmentOps.resize(colorPortsIn.size());
    colorAttachments.resize(colorPortsIn.size());
}
} // namespace Engine::RGraph
