#pragma once
#include "Code/Ptr.hpp"
#include "GfxDriver/Image.hpp"
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
            RefPtr<Image> image;
            MultiSampling multiSampling = MultiSampling::Sample_Count_1;
            AttachmentLoadOperation loadOp = AttachmentLoadOperation::Load;
            AttachmentStoreOperation storeOp = AttachmentStoreOperation::Store;
            AttachmentLoadOperation stencilLoadOp = AttachmentLoadOperation::Load;
            AttachmentStoreOperation stencilStoreOp = AttachmentStoreOperation::Store;
        };

        virtual ~RenderPass(){}

        virtual void AddSubpass(const std::vector<RefPtr<Attachment>>& colors, RefPtr<Attachment> depth) = 0;

    private:
    };
}
