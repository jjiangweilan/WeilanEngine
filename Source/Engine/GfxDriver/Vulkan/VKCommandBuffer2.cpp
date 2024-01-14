#include "VKCommandBuffer2.hpp"
#include "GfxDriver/Vulkan/VKShaderProgram.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "VKBuffer.hpp"

namespace Gfx
{
static bool HasWriteAccessMask(VkAccessFlags flags)
{
    if ((flags & VK_ACCESS_SHADER_WRITE_BIT) != 0 || (flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0 ||
        (flags & VK_ACCESS_TRANSFER_WRITE_BIT) != 0 || (flags & VK_ACCESS_HOST_WRITE_BIT) ||
        (flags & VK_ACCESS_MEMORY_WRITE_BIT))
        return true;

    return false;
}

static bool HasReadAccessMask(VkAccessFlags flags)
{
    if ((flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0 || (flags & VK_ACCESS_INDEX_READ_BIT) != 0 ||
        (flags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) != 0 || (flags & VK_ACCESS_UNIFORM_READ_BIT) != 0 ||
        (flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 || (flags & VK_ACCESS_SHADER_READ_BIT) != 0 ||
        (flags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) != 0 ||
        (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) != 0 || (flags & VK_ACCESS_TRANSFER_READ_BIT) != 0 ||
        (flags & VK_ACCESS_HOST_READ_BIT) != 0 || (flags & VK_ACCESS_MEMORY_READ_BIT) != 0)
        return true;
    return false;
}

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
} // namespace Gfx
