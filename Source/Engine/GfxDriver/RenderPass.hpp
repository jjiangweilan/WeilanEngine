#pragma once
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ImageView.hpp"
#include "GfxEnums.hpp"
#include "Libs/Ptr.hpp"
#include "Utils/Structs.hpp"
#include <optional>
#include <vector>

namespace Gfx
{
union ClearColor
{
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
};

struct ClearDepthStencil
{
    float depth;
    uint32_t stencil;
};

// same as VkClearValue
union ClearValue
{
    ClearColor color;
    ClearDepthStencil depthStencil;
};

struct Attachment
{
    ImageView* imageView; // not optional
    MultiSampling multiSampling = MultiSampling::Sample_Count_1;
    AttachmentLoadOperation loadOp = AttachmentLoadOperation::Load;
    AttachmentStoreOperation storeOp = AttachmentStoreOperation::Store;
    AttachmentLoadOperation stencilLoadOp = AttachmentLoadOperation::DontCare;
    AttachmentStoreOperation stencilStoreOp = AttachmentStoreOperation::DontCare;
};

class RenderPass
{
public:
    virtual ~RenderPass() {}

    virtual void AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) = 0;
    virtual void ClearSubpass() = 0;

private:
};
} // namespace Gfx
