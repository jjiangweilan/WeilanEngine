#pragma once
#include "../CommandBuffer.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKRenderPass.hpp"
#include "VKRenderTarget.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKShaderResource;
class VKDevice;
class VKCommandBuffer : public CommandBuffer
{
public:
    VKCommandBuffer(VkCommandBuffer vkCmdBuf);
    VKCommandBuffer(const VKCommandBuffer& other) = delete;
    ~VKCommandBuffer();

    void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass, const std::vector<Gfx::ClearValue>& clearValues) override;
    void EndRenderPass() override;

    void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) override;
    // renderpass and framebuffer have to be compatible.
    // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
    void BindResource(RefPtr<Gfx::ShaderResource> resource) override;
    void BindVertexBuffer(std::span<const VertexBufferBinding> vertexBufferBindings,
                          uint32_t firstBindingIndex) override;
    void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const ShaderConfig& config) override;
    void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType) override;
    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     uint32_t vertexOffset,
                     uint32_t firstInstance) override;
    void SetViewport(const Viewport& viewport) override;
    void CopyImageToBuffer(RefPtr<Gfx::Image> src,
                           RefPtr<Gfx::Buffer> dst,
                           std::span<BufferImageCopyRegion> regions) override;
    void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) override;
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void NextRenderPass() override;

    void CopyBuffer(RefPtr<Gfx::Buffer> bSrc,
                    RefPtr<Gfx::Buffer> bDst,
                    const std::vector<BufferCopyRegion>& copyRegions) override;
    void CopyBufferToImage(RefPtr<Gfx::Buffer> src,
                           RefPtr<Gfx::Image> dst,
                           std::span<BufferImageCopyRegion> regions) override;
    void Barrier(GPUBarrier* barriers, uint32_t barrierCount) override;
    void Begin() override;
    void End() override;

    VkCommandBuffer GetHandle() const { return vkCmdBuf; }

private:
    VkCommandBuffer vkCmdBuf;
    VKRenderPass* currentRenderPass = nullptr;
    uint32_t currentRenderIndex = -1;

    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    std::vector<VkMemoryBarrier> memoryMemoryBarriers;
};
} // namespace Engine::Gfx
