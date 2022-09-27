#pragma once
#include "Code/Ptr.hpp"
#include "GfxEnums.hpp"
#include <optional>
#include <vector>
namespace Engine::Gfx
{
    union ClearColor
    {
        float       float32[4];
        int32_t     int32[4];
        uint32_t    uint32[4];
    };

    struct ClearDepthStencil
    {
        float       depth;
        uint32_t    stencil;
    };

    // same as VkClearValue
    union ClearValue
    {
        ClearColor color;
        ClearDepthStencil depthStencil;
    };

    class RenderPass
    {
    public:
        struct Attachment
        {
            ImageFormat format;
            MultiSampling multiSampling = MultiSampling::Sample_Count_1;
            AttachmentLoadOperation loadOp = AttachmentLoadOperation::Load;
            AttachmentStoreOperation storeOp = AttachmentStoreOperation::Store;
            AttachmentLoadOperation stencilLoadOp = AttachmentLoadOperation::Load;
            AttachmentStoreOperation stencilStoreOp = AttachmentStoreOperation::Store;
        };

        struct Subpass
        {
            std::vector<uint32_t> colors;
            std::vector<uint32_t> inputs;

            // -1 for not used
            int depthAttachment = -1;
        };

        struct SubpassDependency
        {

        };

        virtual ~RenderPass(){}

        virtual void SetAttachments(const std::vector<Attachment>& colors, std::optional<Attachment> depth) = 0;
        virtual void SetSubpass(const std::vector<Subpass>& subpasses) = 0;
        virtual void AddSubpassDependency(const SubpassDependency& subpassDependency) = 0;
    private:
    };
}
