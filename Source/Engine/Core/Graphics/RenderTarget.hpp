#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include <cinttypes>
#include <optional>
#include <vector>

namespace Engine
{
union ClearColorValue
{
    float float32[4] = {0, 0, 0, 0};
    int32_t int32[4];
    uint32_t uint32[4];
};

struct ClearDepthStencilValue
{
    float depth = 1;
    uint32_t stencil = 0;
};

union ClearValue
{
    ClearColorValue color;
    ClearDepthStencilValue depthStencil;
};

struct RenderPassAttachmentConfig
{
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::DontCare;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::DontCare;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare;

    bool operator==(const RenderPassAttachmentConfig& other) const;
};

struct RenderPassConfig
{
    std::vector<RenderPassAttachmentConfig> colors;
    RenderPassAttachmentConfig depthStencil;

    RenderPassConfig(uint32_t colorSize) : colors(colorSize) {}
    RenderPassConfig() : colors(), depthStencil() {}
    bool operator==(const RenderPassConfig& other) const;
};

struct RenderTargetAttachmentDescription
{
    RenderTargetAttachmentDescription();
    Gfx::ImageFormat format;
    Gfx::MultiSampling multiSampling;
    uint32_t mipLevels;
    ClearValue clearValue;
};

struct RenderTargetDescription
{
    uint32_t width;
    uint32_t height;

    std::vector<RenderTargetAttachmentDescription> colorsDescriptions;
    std::optional<RenderTargetAttachmentDescription> depthStencilDescription = std::nullopt;

    uint32_t GetColorAttachmentCount() const
    {
        uint32_t rtn = colorsDescriptions.size();

        return rtn;
    }

    bool HasDepth() const { return depthStencilDescription.has_value(); }
};

class RenderTarget
{
public:
    RenderTarget();
    virtual ~RenderTarget(){};
    const RenderTargetDescription& GetRenderTargetDescription();
    virtual void SetRenderTargetDescription(const RenderTargetDescription& renderTargetDescription) = 0;

protected:
    RenderTargetDescription rtDescription;
};
} // namespace Engine
