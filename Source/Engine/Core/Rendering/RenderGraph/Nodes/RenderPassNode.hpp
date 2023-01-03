#pragma once
#include "../Node.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include <optional>
#include <span>
#include <tuple>
namespace Engine::RGraph
{

struct DrawIndex
{
    uint32_t elementCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;
};

struct IndexBuffer
{
    Gfx::Buffer* buffer;
    uint64_t offset = 0;
    IndexBufferType bufferType = IndexBufferType::UInt16;
};

struct PushConstant
{
    Gfx::ShaderProgram* shaderProgram;
    glm::mat4 mat0;
    glm::mat4 mat1;
};

struct DrawData
{
    Gfx::ShaderProgram* shader = nullptr;
    Gfx::ShaderConfig* shaderConfig = nullptr;
    Gfx::ShaderResource* shaderResource = nullptr;
    std::optional<IndexBuffer> indexBuffer = std::nullopt;
    std::optional<std::vector<VertexBufferBinding>> vertexBuffer = std::nullopt;
    std::optional<PushConstant> pushConstant = std::nullopt;
    std::optional<Rect2D> scissor = std::nullopt;
    std::optional<DrawIndex> drawIndexed = std::nullopt;
};

class RenderPassNode : Node
{
public:
    struct AttachmentOps
    {
        Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
        Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Load;
        Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare;
        Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare;

        void FillAttachment(Gfx::RenderPass::Attachment& a)
        {
            a.multiSampling = multiSampling;
            a.loadOp = loadOp;
            a.storeOp = storeOp;
            a.stencilLoadOp = stencilLoadOp;
            a.stencilStoreOp = stencilStoreOp;
        }
    };

    RenderPassNode();
    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) override;

    Port* GetPortColorIn(int index)
    {
        if (index <= colorPortsIn.size()) return colorPortsIn[index];
        else return nullptr;
    }

    Port* GetPortColorOut(int index)
    {
        if (index <= colorPortsOut.size()) return colorPortsOut[index];
        else return nullptr;
    }

    void SetColorCount(uint32_t count);
    auto& GetDrawList() { return drawList; }
    auto& GetClearValues() { return clearValues; }

    Port* GetPortDepthIn() { return depthPortIn; }
    Port* GetPortDepthOut() { return depthPortOut; }
    Port* GetPortDependentAttachmentsIn() { return dependentAttachmentIn; }
    std::span<AttachmentOps> GetColorAttachmentOps() { return colorAttachmentOps; }
    AttachmentOps& GetDepthAttachmentOp() { return depthAttachmentOp; }

private:
    Port* dependentAttachmentIn;

    std::vector<DrawData> drawList;

    /**
     * describe color attachment operations in render pass
     */
    std::vector<AttachmentOps> colorAttachmentOps;

    /**
     * describe depth attachment operation
     */
    AttachmentOps depthAttachmentOp;

    /**
     * representation of the color ports of the node
     */
    std::vector<Port*> colorPortsIn;
    std::vector<Port*> colorPortsOut;

    /**
     * representation of the depth port of the node
     */
    Port* depthPortIn;
    Port* depthPortOut;

    /* Compiled data */
    UniPtr<Gfx::RenderPass> renderPass;
    CommandBuffer* cmdBuf;

    /**
     * clearValues used in Execute
     */
    std::vector<Gfx::ClearValue> clearValues;

    /**
     * used by renderPass in Execute,
     * length is determined in Compile
     */
    std::vector<Gfx::RenderPass::Attachment> colorAttachments;
    std::optional<Gfx::RenderPass::Attachment> depthAttachment;
};
} // namespace Engine::RGraph
