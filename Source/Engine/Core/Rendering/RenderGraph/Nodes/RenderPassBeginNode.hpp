#pragma once
#include "../Node.hpp"
#include <optional>
#include <span>

namespace Engine::RGraph
{
class RenderPassBeginNode : Node
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

    RenderPassBeginNode();
    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

    Port* GetPortColor(int index)
    {
        if (index <= colorPorts.size()) return colorPorts[index];
        else return nullptr;
    }

    void SetColorCount(uint32_t count);

    Port* GetPortDepth() { return renderPassOut; }
    Port* GetPortRenderPassOut() { return renderPassOut; }
    std::span<AttachmentOps> GetColorAttachmentOps() { return colorAttachmentOps; }
    AttachmentOps& GetDepthAttachmentOp();

private:
    Port* commandBufferPortIn;
    Port* commandBufferPortOut;

    /**
     * describe color attachment operations in render pass
     */
    std::vector<AttachmentOps> colorAttachmentOps;

    /**
     * describe depth attachment operation
     */
    AttachmentOps depthAttachmentOp;
    /**
     * renderPass output port
     */
    Port* renderPassOut;

    /**
     * representation of the color ports of the node
     */
    std::vector<Port*> colorPorts;

    /**
     * representation of the depth port of the node
     */
    Port* depthPort;

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
