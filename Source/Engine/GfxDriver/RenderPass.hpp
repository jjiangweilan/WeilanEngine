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
    ClearValue() : color(ClearColor{.float32 = {0, 0, 0, 0}}){};
    ClearValue(float x, float y, float z, float w) : color(ClearColor{.float32 = {x, y, z, w}}){};
    ClearValue(int32_t x, int32_t y, int32_t z, int32_t w) : color(ClearColor{.int32 = {x, y, z, w}}){};
    ClearValue(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : color(ClearColor{.uint32 = {x, y, z, w}}){};
    ClearValue(const glm::vec4& v) : color(ClearColor{.float32 = {v.x, v.y, v.z, v.w}}){};
    ClearValue(const glm::ivec4& v) : color(ClearColor{.int32 = {v.x, v.y, v.z, v.w}}){};
    ClearValue(const glm::uvec4& v) : color(ClearColor{.uint32 = {v.x, v.y, v.z, v.w}}){};
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

    // TODO: better split this into SetAttachments and AddSubpass
    virtual void AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) = 0;
    virtual void ClearSubpass() = 0;

private:
};
} // namespace Gfx
