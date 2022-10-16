#include "VKCommandBuffer.hpp"
#include "Internal/VKDevice.hpp"
#include "VKRenderTarget.hpp"
#include "VKShaderProgram.hpp"
#include "VKRenderPass.hpp"
#include "VKFrameBuffer.hpp"
#include "VKShaderResource.hpp"
#include "VKBuffer.hpp"
#include <spdlog/spdlog.h>
namespace Engine::Gfx
{
    VKCommandBuffer::VKCommandBuffer() 
    {

    }

    VKCommandBuffer::~VKCommandBuffer()
    {

    }

    void VKCommandBuffer::BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass, const std::vector<Gfx::ClearValue>& clearValues)
    {
        assert(renderPass != nullptr);

        Gfx::VKRenderPass* vRenderPass = static_cast<Gfx::VKRenderPass*>(renderPass.Get());
        VkRenderPass vkRenderPass;
        VkFramebuffer vkFramebuffer;
        vRenderPass->GetHandle(vkRenderPass, vkFramebuffer);
        recordContext.currentPass = vkRenderPass;

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            // make sure all the resource needed are in the correct format before the render pass begin
            // the first need of this is because we need a way to implictly make sure RT from previous render pass 
            // are correctly transformed into correct memory layout
            auto res = recordContext.bindedResources[vkRenderPass];
            for(auto& r : res)
            {
                r->PrepareResource(cmd);
            }

            auto extent = vRenderPass->GetExtent();
            VkRenderPassBeginInfo renderPassBeginInfo;
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.pNext = VK_NULL_HANDLE;
            renderPassBeginInfo.renderPass = vkRenderPass;
            renderPassBeginInfo.framebuffer = vkFramebuffer;
            renderPassBeginInfo.renderArea= {{0, 0}, {extent.width, extent.height}};
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

            vRenderPass->TransformAttachmentIfNeeded(cmd);
            vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::EndRenderPass()
    {
        recordContext.currentPass = nullptr;
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdEndRenderPass(cmd);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::AppendCustomCommand(std::function<void(VkCommandBuffer)>&& ff)
    {
        auto f = [=, customF = std::move(ff)](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            customF(cmd);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::RecordToVulkanCmdBuf(VkCommandBuffer cmd)
    {
        executeContext.currentPass = VK_NULL_HANDLE;
        executeContext.subpass = 0;

        for(auto& f : pendingCommands)
        {
            f(cmd, executeContext, recordContext);
        }

        pendingCommands.clear();
    }

    void VKCommandBuffer::Blit(RefPtr<Gfx::Image> from_Base, RefPtr<Gfx::Image> to_Base)
    {
        VKImage* from = static_cast<VKImage*>(from_Base.Get());
        VKImage* to = static_cast<VKImage*>(to_Base.Get());

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext) mutable
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
        if (resource_ == nullptr) return;

        VKShaderResource* resource = (VKShaderResource*)resource_.Get();
        if (recordContext.currentPass != VK_NULL_HANDLE)
        {
            recordContext.bindedResources[recordContext.currentPass].push_back(resource);
        }

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext) mutable
        {
            VkDescriptorSet descSet = resource->GetDescriptorSet();
            resource->PrepareResource(cmd);
            if (descSet != VK_NULL_HANDLE)
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ((VKShaderProgram*)resource->GetShader().Get())->GetVKPipelineLayout(), resource->GetDescriptorSetSlot(), 1, &descSet, 0, VK_NULL_HANDLE);
        };
        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindShaderProgram(RefPtr<Gfx::ShaderProgram> program_, const ShaderConfig& config)
    {
        VKShaderProgram* program = (VKShaderProgram*)program_.Get();

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
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

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdSetScissor(cmd, firstScissor, scissorCount, vkRects);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindVertexBuffer(const std::vector<RefPtr<Gfx::GfxBuffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex)
    {
        assert(buffers.size() < 16);
        VkBuffer vkBuffers[16];
        uint64_t vkOffsets[16];
        for(uint32_t i = 0; i < buffers.size(); ++i)
        {
            vkBuffers[i] = static_cast<VKBuffer*>(buffers[i].Get())->GetVKBuffer();
            vkOffsets[i] = offsets[i];
        }

        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdBindVertexBuffers(cmd, firstBindingIndex, buffers.size(), vkBuffers, vkOffsets);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::BindIndexBuffer(RefPtr<Gfx::GfxBuffer> buffer, uint64_t offset, IndexBufferType indexBufferType)
    {
        VkIndexType indexType = indexBufferType == IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            VkBuffer indexBuf = static_cast<VKBuffer*>(buffer.Get())->GetVKBuffer();
            vkCmdBindIndexBuffer(cmd, indexBuf, offset, indexType);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        auto f = [=](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstIndex);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        auto f = [=](VkCommandBuffer cmdBuf, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdDraw(cmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
        };

        pendingCommands.push_back(std::move(f));
    }

    void VKCommandBuffer::NextRenderPass()
    {
        auto f = [=](VkCommandBuffer cmdBuf, ExecuteContext& context, RecordContext& recordContext)
        {
            context.subpass += 1;
            vkCmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_INLINE);
        };

        pendingCommands.push_back(std::move(f));

    }

    void VKCommandBuffer::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram_, void* data)
    {
        VKShaderProgram* shaderProgram = static_cast<VKShaderProgram*>(shaderProgram_.Get());

        VkShaderStageFlags stages = 0;
        uint32_t totalSize = 0;
        for(auto& ps : shaderProgram->GetShaderInfo().pushConstants)
        {
            auto& pushConstant = ps.second;
            stages |= ShaderInfo::Utils::MapShaderStage(pushConstant.stages);
            totalSize += pushConstant.data.size;
        }

        // std::function requires the callables to be copy constructable, thus requires it's member to be copy constructable
        // so we can't simple use a UniPtr https://stackoverflow.com/questions/32486623/moving-a-lambda-once-youve-move-captured-a-move-only-type-how-can-the-lambda
        // TODO: this has to be optimized away, the ptr may lost tracking
        char* byteData = new char[totalSize];
        memcpy(byteData, data, totalSize);

        auto f = [=, byteData = std::move(byteData)](VkCommandBuffer cmd, ExecuteContext& context, RecordContext& recordContext)
        {
            vkCmdPushConstants(cmd, shaderProgram->GetVKPipelineLayout(), stages, 0, totalSize, byteData);
            delete byteData;
        };

        pendingCommands.push_back(std::move(f));
    }
}
