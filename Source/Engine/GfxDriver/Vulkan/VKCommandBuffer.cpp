#include "VKCommandBuffer.hpp"
#include "Internal/VKDevice.hpp"
#include "VKRenderTarget.hpp"
#include "VKShaderProgram.hpp"
#include "VKRenderPass.hpp"
#include "VKFrameBuffer.hpp"
#include "Exp/VKShaderResource.hpp"
#include "Exp/VKBuffer.hpp"
#include <spdlog/spdlog.h>
namespace Engine::Gfx
{
    VKCommandBuffer::VKCommandBuffer() 
    {

    }

    VKCommandBuffer::~VKCommandBuffer()
    {

    }

    void VKCommandBuffer::BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass, RefPtr<Gfx::FrameBuffer> frameBuffer, const std::vector<Gfx::ClearValue>& clearValues)
    {
        assert(renderPass != nullptr);
        assert(frameBuffer != nullptr);

        Gfx::VKRenderPass* vRenderPass = static_cast<Gfx::VKRenderPass*>(renderPass.Get());
        Gfx::VKFrameBuffer* vFrameBuffer = static_cast<Gfx::VKFrameBuffer*>(frameBuffer.Get());
        recordContext.currentPass = vRenderPass->GetHandle();

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            // make sure all the resource needed are in the correct format before the render pass begin
            // the first need of this is because we need a way to implictly make sure RT from previous render pass 
            // are correctly transformed into correct memory layout
            auto res = recordContext.bindedResources[vRenderPass->GetHandle()];
            for(auto& r : res)
            {
                r->PrepareResource(cmd);
            }

            VkRenderPassBeginInfo renderPassBeginInfo;

            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.pNext = VK_NULL_HANDLE;
            renderPassBeginInfo.renderPass = vRenderPass->GetHandle();
            renderPassBeginInfo.framebuffer = vFrameBuffer->GetHandle();
            renderPassBeginInfo.renderArea= {{0, 0}, {vFrameBuffer->GetWidth(), vFrameBuffer->GetHeight()}};
            renderPassBeginInfo.clearValueCount = clearValues.size();
            VkClearValue vkClearValues[16];
            int i = 0;
            for(auto& v : clearValues)
            {
                memcpy(&vkClearValues[i].color, &v.color, sizeof(v.color));
                vkClearValues[i].depthStencil.stencil = v.depthStencil.stencil;
                vkClearValues[i].depthStencil.depth = v.depthStencil.depth;
                ++i;
            }
            renderPassBeginInfo.pClearValues = vkClearValues;
            context.currentPass = renderPassBeginInfo.renderPass;

            vRenderPass->TransformAttachmentIfNeeded(cmd, vFrameBuffer->GetAttachments());
            vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::EndRenderPass()
    {
        recordContext.currentPass = nullptr;
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            vkCmdEndRenderPass(cmd);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::AppendCustomCommand(std::function<void(VkCommandBuffer)>&& ff)
    {
        auto f = [=, customF = std::move(ff)](VkCommandBuffer cmd, ExecuteContext& context)
        {
            customF(cmd);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::ExecutePendingCommands(VkCommandBuffer cmd)
    {
        executeContext.currentPass = VK_NULL_HANDLE;
        executeContext.subpass = 0;

        for(auto& f : pendingCommands)
        {
            f(cmd, executeContext);
        }

        pendingCommands.clear();
    }

    void VKCommandBuffer::Render(Mesh& mesh, Material& material)
    {
        Mesh* pMesh = &mesh;
        Exp::VKShaderResource* shaderResource = static_cast<Exp::VKShaderResource*>(material.GetShaderResource().Get());
        VKShaderProgram* shader = static_cast<VKShaderProgram*>(material.GetShader()->GetShaderProgram().Get());

        BindResource(shaderResource);
        auto f = [=, &material](VkCommandBuffer cmd, ExecuteContext& context) mutable
        {
            // binding pipeline
            auto pipeline = shader->RequestPipeline(material.GetShaderConfig(), context.currentPass, context.subpass);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            // binding mesh
            const auto& meshBindingInfo = pMesh->GetMeshBindingInfo();
            VkBuffer buffers[32];
            uint32_t i = 0;
            for(auto b : meshBindingInfo.bindingBuffers)
            {
                buffers[i] = static_cast<Exp::VKBuffer*>(b.Get())->GetVKBuffer();
                i += 1;
            }
            vkCmdBindVertexBuffers(cmd, 0, meshBindingInfo.bindingBuffers.size(), buffers, meshBindingInfo.bindingOffsets.data());
            VkBuffer indexBuf = static_cast<Exp::VKBuffer*>(meshBindingInfo.indexBuffer.Get())->GetVKBuffer();
            vkCmdBindIndexBuffer(cmd, indexBuf, meshBindingInfo.indexBufferOffset, VK_INDEX_TYPE_UINT16);

            auto& vertDesc = pMesh->GetVertexDescription();
            vkCmdDrawIndexed(cmd, vertDesc.index.count, 1, 0, 0, 0);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::Blit(RefPtr<Gfx::Image> from_Base, RefPtr<Gfx::Image> to_Base)
    {
        VKImage* from = static_cast<VKImage*>(from_Base.Get());
        VKImage* to = static_cast<VKImage*>(to_Base.Get());

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context) mutable
        {
            from->TransformLayoutIfNeeded(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            to->TransformLayoutIfNeeded(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
            VkImageBlit blit;
            blit.dstOffsets[0] = {0,0,0};
            blit.dstOffsets[1] = {(int32_t)from->GetDescription().width, (int32_t)from->GetDescription().height, 1};
            VkImageSubresourceLayers dstLayers;
            dstLayers.aspectMask = from->GetDefaultSubresourceRange().aspectMask;
            dstLayers.baseArrayLayer = 0;
            dstLayers.layerCount = from->GetDefaultSubresourceRange().layerCount;
            dstLayers.mipLevel = 0;
            blit.dstSubresource = dstLayers;

            blit.srcOffsets[0] = {0,0,0};
            blit.srcOffsets[1] = {(int32_t)to->GetDescription().width, (int32_t)to->GetDescription().height, 1};
            blit.srcSubresource = dstLayers; // basically copy the resources from dst without much configuration
        
            vkCmdBlitImage(cmd, from->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindResource(RefPtr<Gfx::ShaderResource> resource_)
    {
        Exp::VKShaderResource* resource = (Exp::VKShaderResource*)resource_.Get();
        if (recordContext.currentPass != VK_NULL_HANDLE)
        {
            recordContext.bindedResources[recordContext.currentPass].push_back(resource);
        }

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context) mutable
        {
            VkDescriptorSet descSet = resource->GetDescriptorSet();
            resource->PrepareResource(cmd);
            if (descSet != VK_NULL_HANDLE)
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ((VKShaderProgram*)resource->GetShader().Get())->GetVKPipelineLayout(), resource->GetDescriptorSetSlot(), 1, &descSet, 0, VK_NULL_HANDLE);
            resource->UpdatePushConstant(cmd);

        };
        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindShaderProgram(RefPtr<Gfx::ShaderProgram> program_, const ShaderConfig& config)
    {
        VKShaderProgram* program = (VKShaderProgram*)program_.Get();

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            // binding pipeline
            auto pipeline = program->RequestPipeline(config, context.currentPass, context.subpass);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
    {
        assert(scissorCount < 8);

        VkRect2D vkRects[8];
        for(uint32_t i = 0; i < scissorCount; ++i)
        {
            vkRects[i].offset.x = rect->offset.x;
            vkRects[i].offset.y = rect->offset.y;
            vkRects[i].extent.width = rect->extent.width;
            vkRects[i].extent.height = rect->extent.height;
        }

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            vkCmdSetScissor(cmd, firstScissor, scissorCount, vkRects);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindVertexBuffer(const std::vector<RefPtr<Gfx::Buffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex)
    {
        assert(buffers.size() < 16);
        VkBuffer vkBuffers[16];
        uint64_t vkOffsets[16];
        for(uint32_t i = 0; i < buffers.size(); ++i)
        {
            vkBuffers[i] = static_cast<Exp::VKBuffer*>(buffers[i].Get())->GetVKBuffer();
            vkOffsets[i] = offsets[i];
        }

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            vkCmdBindVertexBuffers(cmd, firstBindingIndex, buffers.size(), vkBuffers, vkOffsets);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindIndexBuffer(Gfx::Buffer* buffer, uint64_t offset)
    {
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            VkBuffer indexBuf = static_cast<Exp::VKBuffer*>(buffer)->GetVKBuffer();
            vkCmdBindIndexBuffer(cmd, indexBuf, offset, VK_INDEX_TYPE_UINT16);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context)
        {
            vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstIndex);
        };

        pendingCommands.push_back(std::move(f));
    }
}
