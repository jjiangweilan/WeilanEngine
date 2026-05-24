#include "VKCommandBuffer.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKRenderPass.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKShaderProgram.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKShaderResource.hpp"
#include "Engine/Library/Assert.hpp"
#include "VKBuffer.hpp"
#include "VKCommandBufferProcessor.hpp"
#include "VKImage.hpp"

namespace Gfx
{

void VKCommandBuffer::BeginRenderPass(Gfx::RenderPass_Deprecated& renderPass, std::span<Gfx::ClearValue> clearValues)
{
    ASSERT(clearValues.size() <= 8);

    VKBeginRenderPassCmd cmd{};

    if (validationCheck && !renderPass.RenderPassRenderingValidationCheck())
    {
        spdlog::critical(
            "failed to execute render pass because it's not ready for rendering, you need to set subpass "
            "and also set attachments in subpass"
        );
    }

    cmd.renderPass = static_cast<VKRenderPass*>(&renderPass);
    for (int i = 0; i < clearValues.size() && i < 8; ++i)
    {
        memcpy(cmd.clearValues, clearValues.data(), clearValues.size() * sizeof(Gfx::ClearValue));
    }
    cmd.clearValueCount = clearValues.size();

    cmds.push_back(VKCmd{VKCmdType::BeginRenderPass, cmd});
}

void VKCommandBuffer::EndRenderPass()
{
    VKEndRenderPassCmd cmd{};
    cmds.push_back(VKCmd{VKCmdType::EndRenderPass, cmd});
}

void VKCommandBuffer::DrawIndirect(BufferIdentifier buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    VKDrawIndirectCmd cmd{};

    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.drawCount = drawCount;
    cmd.stride = stride;

    cmds.push_back(VKCmd{VKCmdType::DrawIndirect, cmd});
}

void VKCommandBuffer::DrawIndexedIndirect(BufferIdentifier buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    VKDrawIndexedIndirectCmd cmd{};

    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.drawCount = drawCount;
    cmd.stride = stride;

    cmds.push_back(VKCmd{VKCmdType::DrawIndexedIndirect, cmd});
}

void VKCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    VKDrawCmd cmd{};

    cmd.vertexCount = vertexCount;
    cmd.instanceCount = instanceCount;
    cmd.firstVertex = firstVertex;
    cmd.firstInstance = firstInstance;

    cmds.push_back(VKCmd{VKCmdType::Draw, cmd});
}

void VKCommandBuffer::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
)
{
    VKDrawIndexedCmd cmd{};
    cmd.indexCount = indexCount;
    cmd.instanceCount = instanceCount;
    cmd.firstIndex = firstIndex;
    cmd.vertexOffset = vertexOffset;
    cmd.firstInstance = firstInstance;

    cmds.push_back(VKCmd{VKCmdType::DrawIndexed, cmd});
}

void VKCommandBuffer::BindResource(uint32_t set, Gfx::ShaderResource* resource)
{
    if (set > 4)
        return;

    VKBindResourceCmd cmd{};
    cmd.set = set;
    cmd.resource = static_cast<VKShaderResource*>(resource);

    cmds.push_back(VKCmd{VKCmdType::BindResource, cmd});
}

void VKCommandBuffer::BindShaderProgram(RefPtr<Gfx::ShaderProgram> bProgram, const PipelineConfig& config)
{
    VKBindShaderProgramCmd cmd{};

    cmd.program = (VKShaderProgram*)bProgram.Get();
    cmd.config = config;

    cmds.push_back(VKCmd{VKCmdType::BindShaderProgram, cmd});
}

void VKCommandBuffer::BindVertexBuffer(
    std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
)
{
    ASSERT(vertexBufferBindings.size() <= 8);

    for (int i = 0; i < vertexBufferBindings.size() && i < 8; ++i)
    {
        if (vertexBufferBindings[i].buffer.IsTemporary())
        {
            SPDLOG_ERROR("CommandBuffer::BindVertexBuffer: temporary vertex buffers are not supported yet");
            return;
        }
    }

    VKBindVertexBufferCmd cmd{};
    for (int i = 0; i < vertexBufferBindings.size() && i < 8; ++i)
        cmd.vertexBufferBindings[i] = vertexBufferBindings[i];
    cmd.firstBindingIndex = firstBindingIndex;
    cmd.vertexBufferBindingCount = vertexBufferBindings.size();

    cmds.push_back(VKCmd{VKCmdType::BindVertexBuffer, cmd});
}

void VKCommandBuffer::BindIndexBuffer(BufferIdentifier buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType)
{
    VKBindIndexBufferCmd cmd{};

    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.indexType = indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    cmds.push_back(VKCmd{VKCmdType::BindIndexBuffer, cmd});
}

void VKCommandBuffer::SetViewport(const Viewport& viewport)
{
    VKSetViewportCmd cmd{};
    VkViewport v{
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.width,
        .height = viewport.height,
        .minDepth = viewport.minDepth,
        .maxDepth = viewport.maxDepth
    };
    cmd.viewport = v;
    cmds.push_back(VKCmd{VKCmdType::SetViewport, cmd});
}

void VKCommandBuffer::CopyImageToBuffer(
    RefPtr<Gfx::Image> src, BufferIdentifier dst, std::span<BufferImageCopyRegion> regions
)
{
    ASSERT(regions.size() < 8);
    VKCopyImageToBufferCmd cmd{};

    cmd.src = static_cast<VKImage*>(src.Get());
    cmd.dst = dst;
    for (int i = 0; i < regions.size() && i < 8; ++i)
        cmd.regions[i] = regions[i];

    cmd.regionsCount = regions.size();

    cmds.push_back(VKCmd{VKCmdType::CopyImageToBuffer, cmd});
};

void VKCommandBuffer::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data)
{
    VKSetPushConstantCmd cmd{};
    cmd.shaderProgram = static_cast<VKShaderProgram*>(shaderProgram.Get());

    uint32_t totalSize = 0;
    cmd.stages = 0;
    for (const auto& ps : shaderProgram->GetShaderInfo().pushConstants)
    {
        cmd.stages |= MapShaderStages(ps.stages);
        totalSize += ps.size;
    }
    cmd.dataSize = totalSize;
    memcpy(cmd.data, data, totalSize < 128 ? totalSize : 128);
    cmds.push_back(VKCmd{VKCmdType::SetPushConstant, cmd});
};
void VKCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    ASSERT(scissorCount <= 8);

    VKSetScissorCmd cmd{};
    cmd.firstScissor = firstScissor;
    cmd.scissorCount = scissorCount;

    for (int i = 0; i < scissorCount; ++i)
    {
        cmd.rects[i].offset.x = rect->offset.x;
        cmd.rects[i].offset.y = rect->offset.y;
        cmd.rects[i].extent.width = rect->extent.width;
        cmd.rects[i].extent.height = rect->extent.height;
    }

    memcpy(cmd.rects, rect, scissorCount);
    cmds.push_back(VKCmd{VKCmdType::SetScissor, cmd});
};
void VKCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VKDispatchCmd cmd{};
    cmd.groupCountX = groupCountX;
    cmd.groupCountY = groupCountY;
    cmd.groupCountZ = groupCountZ;

    cmds.push_back(VKCmd{VKCmdType::Dispatch, cmd});
};
void VKCommandBuffer::DispatchIndirect(BufferIdentifier buffer, size_t bufferOffset)
{
    VKDispatchIndirectCmd cmd{};
    cmd.buffer = buffer;
    cmd.bufferOffset = bufferOffset;

    cmds.push_back(VKCmd{VKCmdType::DispatchIndirect, cmd});
};
void VKCommandBuffer::NextRenderPass()
{
    VKNextRenderPassCmd cmd{};

    cmds.push_back(VKCmd{VKCmdType::NextRenderPass, cmd});
};
void VKCommandBuffer::PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings)
{
    ASSERT(bindings.size() <= 8);
    VKPushDescriptorCmd cmd{};
    cmd.shader = static_cast<VKShaderProgram*>(&shader);
    cmd.set = set;
    cmd.bindingCount = bindings.size() <= 8 ? bindings.size() : 8;
    memcpy(cmd.bindings, bindings.data(), cmd.bindingCount * sizeof(DescriptorBinding));

    cmds.push_back({VKCmd{VKCmdType::PushDescriptorSet, cmd}});
};

void VKCommandBuffer::CopyBuffer(
    BufferIdentifier bSrc, BufferIdentifier bDst, std::span<BufferCopyRegion> copyRegions
)
{
    ASSERT(copyRegions.size() <= 8);
    VKCopyBufferCmd cmd{};
    cmd.src = bSrc;
    cmd.dst = bDst;
    cmd.copyRegionCount = copyRegions.size();
    for (int i = 0; i < copyRegions.size(); ++i)
    {
        auto& c = copyRegions[i];
        cmd.copyRegions[i] = {c.srcOffset, c.dstOffset, c.size};
    }
    cmds.push_back(VKCmd{VKCmdType::CopyBuffer, cmd});
};
void VKCommandBuffer::CopyBufferToImage(
    BufferIdentifier src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
)
{
    ASSERT(regions.size() <= 8);
    VKCopyBufferToImageCmd cmd{};
    cmd.src = src;
    cmd.dst = static_cast<VKImage*>(dst.Get());
    cmd.regionCount = regions.size();
    for (int i = 0; i < regions.size() && i < 8; ++i)
    {
        auto& r = regions[i];
        cmd.regions[i] = VkBufferImageCopy{
            .bufferOffset = r.bufferOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = VkImageSubresourceLayers{
                .aspectMask = static_cast<VkImageAspectFlags>(r.layers.aspectMask),
                .mipLevel = r.layers.mipLevel,
                .baseArrayLayer = r.layers.baseArrayLayer,
                .layerCount = r.layers.layerCount,
            },
            .imageOffset = VkOffset3D{r.offset.x, r.offset.y, r.offset.z},
            .imageExtent = VkExtent3D{r.extend.width, r.extend.height, r.extend.depth},
        };
    }

    cmds.push_back(VKCmd{VKCmdType::CopyBufferToImage, cmd});
};

void VKCommandBuffer::Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp)
{
    VKBlitCmd cmd{};
    cmd.from = static_cast<VKImage*>(from.Get());
    cmd.to = static_cast<VKImage*>(to.Get());
    cmd.blitOp = blitOp;

    cmds.push_back(VKCmd{VKCmdType::Blit, cmd});
}

void VKCommandBuffer::PresentImage(VKImage* image)
{
    VKPresentCmd cmd{};
    cmd.image = image;
    cmds.push_back({VKCmdType::Present, cmd});
}

void VKCommandBuffer::SetTexture(
    ShaderBindingHandle handle, int index, ImageIdentifier id, std::optional<ImageViewOption> imageViewOption
)
{
    if (id.GetType() == ImageIdentifier::Type::Image)
    {
        VKSetTextureCmd cmd{};

        cmd.handle = handle;
        cmd.image = id.GetAsImage();
        cmd.index = index;
        cmd.imageViewOption = imageViewOption;

        cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
    }
    else if (id.GetType() == ImageIdentifier::Type::ImageView)
    {
        ASSERT(false && "use Texture+imageViewOption");
    }
    else if (id.GetType() == ImageIdentifier::Type::Handle)
    {
        auto image = graph->GetImage(id.GetAsUUID());

        VKSetTextureCmd cmd{};

        cmd.handle = handle;
        cmd.image = image;
        cmd.index = index;
        cmd.imageViewOption = imageViewOption;

        cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
    }
}

void VKCommandBuffer::SetTexture(
    ShaderBindingHandle handle, int index, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption
)
{
    VKSetTextureCmd cmd{};

    cmd.handle = handle;
    cmd.image = &image;
    cmd.index = index;
    cmd.imageViewOption = imageViewOption;

    cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
}

void VKCommandBuffer::SetBuffer(ShaderBindingHandle handle, int index, BufferIdentifier buffer)
{
    VKSetBufferCmd cmd{};

    cmd.handle = handle;
    cmd.buffer = buffer;
    cmd.index = index;

    cmds.push_back(VKCmd{VKCmdType::SetBuffer, cmd});
}

void VKCommandBuffer::AllocateAttachment(const ImageIdentifier& id, RenderImageDescriptor& desc)
{
    graph->Request(id, desc);
}

void VKCommandBuffer::BeginRenderPass(RenderPass& renderPass, std::span<ClearValue> clearValues)
{
    VKRGBeginRenderPassCmd cmd{};

    if (validationCheck)
    {
        if (renderPass.GetAttachments().size() != clearValues.size() || !renderPass.IsValidForRendering())
        {
            throw std::runtime_error("");
        }
    }

    cmd.renderPass = renderPass;
    int copySize = clearValues.size() <= 8 ? clearValues.size_bytes() : 8 * sizeof(ClearValue);
    memcpy(cmd.clearValues, clearValues.data(), copySize);
    cmd.clearValueCount = clearValues.size() <= 8 ? clearValues.size() : 8;

    cmds.push_back(VKCmd{VKCmdType::RGBeginRenderPass, cmd});
}

void VKCommandBuffer::Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp)
{
    VKBlitCmd cmd{};

    VKImage* from = nullptr;
    VKImage* to = nullptr;
    if (src.GetType() == ImageIdentifier::Type::Image)
    {
        from = static_cast<VKImage*>(src.GetAsImage());
    }
    else if (src.GetType() == ImageIdentifier::Type::ImageView)
    {
        ASSERT(false && "not implemented");
    }
    else if (src.GetType() == ImageIdentifier::Type::Handle)
    {
        from = graph->GetImage(src.GetAsUUID());
    }

    if (dst.GetType() == ImageIdentifier::Type::Image)
    {
        to = static_cast<VKImage*>(dst.GetAsImage());
    }
    else if (src.GetType() == ImageIdentifier::Type::ImageView)
    {
        ASSERT(false && "not implemented");
    }
    else if (src.GetType() == ImageIdentifier::Type::Handle)
    {
        to = graph->GetImage(dst.GetAsUUID());
    }

    cmd.from = static_cast<VKImage*>(from);
    cmd.to = static_cast<VKImage*>(to);
    cmd.blitOp = blitOp;

    cmds.push_back(VKCmd{VKCmdType::Blit, cmd});
}

void VKCommandBuffer::BeginLabel(std::string_view label, const glm::float4& color)
{
    if (beginLabelStarted)
    {
        spdlog::error("Starting an unmatched label {}, previous label: ", label, currentLabel);
        beginLabelStarted = false;
        return;
    }

    ENGINE_BEGIN_PROFILE(label);
    VKBeginLabelCmd cmd{};
    currentLabel = label;

    cmd.label = std::string(label);
    memcpy(cmd.color, &color[0], sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::BeginLabel, cmd});
}

void VKCommandBuffer::BeginLabel(std::string_view label, float color[4])
{
    ENGINE_BEGIN_PROFILE(label);

    VKBeginLabelCmd cmd{};

    cmd.label = std::string(label);
    memcpy(cmd.color, color, sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::BeginLabel, cmd});
}
void VKCommandBuffer::EndLabel()
{
    ENGINE_END_PROFILE;

    VKCmd cmd{VKCmdType::EndLabel};

    cmds.push_back(cmd);
}
void VKCommandBuffer::InsertLabel(std::string_view label, float color[4])
{
    VKInsertLabelCmd cmd{};

    cmd.label = std::string(label);
    memcpy(cmd.color, color, sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::InsertLabel, cmd});
}

void VKCommandBuffer::InsertLabel(std::string_view label, const glm::float4& color)
{
    VKInsertLabelCmd cmd{};

    cmd.label = label;
    memcpy(cmd.color, &color[0], sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::InsertLabel, cmd});
}

void VKCommandBuffer::SetLineWidth(float lineWidth)
{
    VKSetLineWidthCmd cmd{};

    cmd.lineWidth = lineWidth;

    cmds.push_back(VKCmd{VKCmdType::SetLineWidth, cmd});
}

void VKCommandBuffer::SetDepthBias(float constantFactor, float clamp, float slopeFactor)
{
    VKSetDepthBiasCmd cmd{};

    cmd.constantFactor = constantFactor;
    cmd.clamp = clamp;
    cmd.slopeFactor = slopeFactor;

    cmds.push_back(VKCmd{VKCmdType::SetDepthBias, cmd});
}

void VKCommandBuffer::SetDepthBiasEnable(bool enable)
{
    VKSetDepthBiasEnableCmd cmd{};

    cmd.enable = enable;

    cmds.push_back(VKCmd{VKCmdType::SetDepthBiasEnable, cmd});
}

std::shared_ptr<AsyncReadbackHandle> VKCommandBuffer::AsyncReadback(BufferIdentifier buffer, size_t size, size_t offset)
{
    VKAsyncReadbackCmd cmd{};

    std::shared_ptr<AsyncReadbackHandle> handle = std::make_shared<AsyncReadbackHandle>();
    cmd.buffer = buffer;
    cmd.size = size;
    cmd.offset = offset;
    readbacks.push_back(handle);
    cmd.handle = &readbacks.back();

    cmds.push_back(VKCmd{VKCmdType::AsyncReadback, cmd});
    return handle;
}

void VKCommandBuffer::GraphicsBlit(const ImageIdentifier& from, const ImageIdentifier& to)
{
    VKGraphicsBlitCmd cmd{};
    cmd.from = from;
    cmd.to = to;

    cmds.push_back(VKCmd{VKCmdType::GraphicsBlit, cmd});
}

void VKCommandBuffer::ClearColorImage(Image* image, const ClearColor& color)
{
    VKClearColorImageCmd cmd{};
    cmd.image = image;
    cmd.clearValue = color;

    cmds.push_back(VKCmd{VKCmdType::ClearColorImage, cmd});
}

void VKCommandBuffer::UploadData(BufferIdentifier buffer, void* data, size_t dataSize, size_t offset)
{
    VKUploadDataCmd cmd{};
    cmd.dst = buffer;
    cmd.dstOffset = offset;
    cmd.data.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + dataSize);

cmds.push_back(VKCmd{VKCmdType::UploadData, cmd});
}

TemporaryBufferHandle VKCommandBuffer::AllocateBuffer(
    size_t size,
    TemporaryBufferUsage usage,
    size_t alignment
)
{
    if (usage == TemporaryBufferUsage::AccelerationStructure)
    {
        SPDLOG_ERROR("CommandBuffer temporary acceleration-structure buffers are not supported yet");
        return {};
    }

    if (size == 0 || alignment == 0)
    {
        SPDLOG_ERROR("CommandBuffer::AllocateBuffer: size and alignment must be non-zero");
        return {};
    }

    static std::atomic<uint64_t> nextId{1};

    TemporaryBufferHandle handle{};
    handle.id = nextId.fetch_add(1);
    handle.size = size;
    handle.usage = usage;

    VKAllocateBufferCmd cmd{};
    cmd.handle = handle;
    cmd.size = size;
    cmd.alignment = alignment;
    cmd.usage = usage;

    cmds.push_back(VKCmd{VKCmdType::AllocateBuffer, cmd});
    return handle;
}

void VKCommandBuffer::BeginRenderPass(std::span<const RenderAttachment> images, std::span<ClearValue> clearValues)
{
    VKDynamicRenderPassCmd cmd{};
    cmd.imageIdentifiers = std::vector(images.begin(), images.end());
    cmd.clearValues = std::vector(clearValues.begin(), clearValues.end());

    cmds.push_back(VKCmd{VKCmdType::DynamicBeginRenderPass, cmd});
}

void VKCommandBuffer::BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings)
{
    VKDynamicBindResourceCmd cmd{};
    cmd.set = set;
    cmd.bindings = bindings;

    cmds.push_back(VKCmd{VKCmdType::DynamicBindResource, cmd});
}

void VKCommandBuffer::BuildBLAS(RayTracingMeshHandle handle, std::span<VkAccelerationStructureGeometryKHR> geometries, std::span<uint32_t> maxPrimitiveCounts)
{
    VKBuildBLASCmd cmd{};
    cmd.handle = handle;
    cmd.vkGeometries.assign(geometries.begin(), geometries.end());
    cmd.maxPrimitiveCounts.assign(maxPrimitiveCounts.begin(), maxPrimitiveCounts.end());

    cmds.push_back(VKCmd{VKCmdType::BuildBLAS, cmd});
}

void VKCommandBuffer::BuildTLAS(RayTracingSceneHandle handle, std::span<RayTracingInstanceHandle> instances)
{
    VKBuildTLASCmd cmd{};
    cmd.handle = handle;
    cmd.instances.assign(instances.begin(), instances.end());

    cmds.push_back(VKCmd{VKCmdType::BuildTLAS, cmd});
}

} // namespace Gfx
