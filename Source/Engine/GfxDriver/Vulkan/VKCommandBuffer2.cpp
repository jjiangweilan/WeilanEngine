#include "VKCommandBuffer2.hpp"
#include "GfxDriver/Vulkan/VKShaderProgram.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "VKBuffer.hpp"
#include "VKImage.hpp"

namespace Gfx
{

void VKCommandBuffer2::BeginRenderPass(Gfx::RenderPass& renderPass, const std::vector<Gfx::ClearValue>& clearValues)
{
    assert(clearValues.size() <= 8);

    VKCmd cmd{VKCmdType::BeginRenderPass};

    cmd.type = VKCmdType::BeginRenderPass;
    cmd.beginRenderPass.renderPass = static_cast<VKRenderPass*>(&renderPass);
    for (int i = 0; i < clearValues.size() && i < 8; ++i)
        cmd.beginRenderPass.clearValues[i] = clearValues[i];

    cmds.push_back(cmd);
}

void VKCommandBuffer2::EndRenderPass()
{
    VKCmd cmd{VKCmdType::EndRenderPass};
    cmds.push_back(cmd);
}

void VKCommandBuffer2::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
)
{
    VKCmd cmd{VKCmdType::DrawIndexed};
    cmd.drawIndexed.indexCount = indexCount;
    cmd.drawIndexed.instanceCount = instanceCount;
    cmd.drawIndexed.firstIndex = firstIndex;
    cmd.drawIndexed.vertexOffset = vertexOffset;
    cmd.drawIndexed.firstInstance = firstInstance;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindResource(uint32_t set, Gfx::ShaderResource* resource)
{
    if (set > 4)
        return;

    VKCmd cmd{VKCmdType::BindResource};
    cmd.bindResource.set = set;
    cmd.bindResource.resource = static_cast<VKShaderResource*>(resource);

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindShaderProgram(RefPtr<Gfx::ShaderProgram> bProgram, const ShaderConfig& config)
{
    VKCmd cmd{VKCmdType::BindShaderProgram};
    cmd.bindShaderProgram.program = (VKShaderProgram*)bProgram.Get();
    cmd.bindShaderProgram.config = &config;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindVertexBuffer(
    std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
)
{
    assert(vertexBufferBindings.size() <= 8);

    VKCmd cmd{VKCmdType::BindVertexBuffer};
    for (int i = 0; i < vertexBufferBindings.size() && i < 8; ++i)
        cmd.bindVertexBuffer.verteBufferBindings[i] = vertexBufferBindings[i];
    cmd.bindVertexBuffer.firstBindingIndex = firstBindingIndex;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindIndexBuffer(
    RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType
)
{
    VKCmd cmd{VKCmdType::BindIndexBuffer};

    cmd.bindIndexBuffer.buffer = static_cast<VKBuffer*>(buffer.Get());
    cmd.bindIndexBuffer.offset = offset;
    cmd.bindIndexBuffer.indexType =
        indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::SetViewport(const Viewport& viewport)
{
    VKCmd cmd{VKCmdType::SetViewport};
    cmd.setViewport.viewport = viewport;
    cmds.push_back(cmd);
}

void VKCommandBuffer2::CopyImageToBuffer(
    RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
)
{
    assert(regions.size() < 8);
    VKCmd cmd{VKCmdType::CopyImageToBuffer};

    cmd.copyImageToBuffer.src = static_cast<VKImage*>(src.Get());
    cmd.copyImageToBuffer.dst = static_cast<VKBuffer*>(dst.Get());
    for (int i = 0; i < regions.size() && i < 8; ++i)
        cmd.copyImageToBuffer.regions[i] = regions[i];
};

void VKCommandBuffer2::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data)
{
    VKCmd cmd{VKCmdType::SetPushConstant};
    cmd.setPushConstant.shaderProgram = static_cast<VKShaderProgram*>(shaderProgram.Get());

    uint32_t totalSize = 0;
    for (auto& ps : shaderProgram->GetShaderInfo().pushConstants)
    {
        auto& pushConstant = ps.second;
        totalSize += pushConstant.data.size;
    }
    memcpy(cmd.setPushConstant.data, data, totalSize < 128 ? totalSize : 128);
    cmds.push_back(cmd);
};
void VKCommandBuffer2::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    assert(scissorCount <= 8);
    VKCmd cmd{VKCmdType::SetScissor};
    cmd.setScissor.firstScissor = firstScissor;
    cmd.setScissor.scissorCount = scissorCount;
    memcpy(cmd.setScissor.rect, rect, scissorCount);
    cmds.push_back(cmd);
};
void VKCommandBuffer2::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VKCmd cmd{VKCmdType::Dispatch};
    cmd.dispatch.groupCountX = groupCountX;
    cmd.dispatch.groupCountY = groupCountY;
    cmd.dispatch.groupCountZ = groupCountZ;

    cmds.push_back(cmd);
};
void VKCommandBuffer2::DispatchIndir(Buffer* buffer, size_t bufferOffset)
{
    VKCmd cmd{VKCmdType::DispatchIndir};
    cmd.dispatchIndir.buffer = static_cast<VKBuffer*>(buffer);
    cmd.dispatchIndir.bufferOffset = bufferOffset;

    cmds.push_back(cmd);
};
void VKCommandBuffer2::NextRenderPass()
{
    VKCmd cmd{VKCmdType::NextRenderPass};
    cmds.push_back(cmd);
};
void VKCommandBuffer2::PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings)
{
    assert(bindings.size() <= 8);
    VKCmd cmd{VKCmdType::PushDescriptorSet};
    cmd.pushDescriptor.shader = static_cast<VKShaderProgram*>(&shader);
    cmd.pushDescriptor.set = set;
    memcpy(cmd.pushDescriptor.bindings, bindings.data(), bindings.size() <= 8 ? bindings.size() : 8);
    cmds.push_back(cmd);
};

void VKCommandBuffer2::CopyBuffer(
    RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
)
{
    assert(copyRegions.size() <= 8);
    VKCmd cmd{VKCmdType::CopyBuffer};
    cmd.copyBuffer.src = static_cast<VKBuffer*>(bSrc.Get());
    cmd.copyBuffer.dst = static_cast<VKBuffer*>(bDst.Get());
    memcpy(cmd.copyBuffer.copyRegions, copyRegions.data(), copyRegions.size() <= 8 ? copyRegions.size() : 8);
    cmds.push_back(cmd);
};
void VKCommandBuffer2::CopyBufferToImage(
    RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
)
{
    assert(regions.size() < 8);
    VKCmd cmd{VKCmdType::CopyBufferToImage};
    cmd.copyBufferToImage.src = static_cast<VKBuffer*>(src.Get());
    cmd.copyBufferToImage.dst = static_cast<VKImage*>(dst.Get());

    memcpy(cmd.copyBufferToImage.regions, regions.data(), regions.size() <= 8 ? regions.size() : 8);

    cmds.push_back(cmd);
};
} // namespace Gfx
