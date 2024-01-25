#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include <cinttypes>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

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
    ClearValue() : color({.float32 = {0, 0, 0, 0}}) {}
    ClearValue(float x, float y, float z, float w) : color(ClearColorValue{.float32 = {x, y, z, w}}){};
    ClearValue(int32_t x, int32_t y, int32_t z, int32_t w) : color(ClearColorValue{.int32 = {x, y, z, w}}){};
    ClearValue(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : color(ClearColorValue{.uint32 = {x, y, z, w}}){};
    ClearValue(const glm::vec4& v) : color(ClearColorValue{.float32 = {v.x, v.y, v.z, v.w}}){};
    ClearValue(const glm::ivec4& v) : color(ClearColorValue{.int32 = {v.x, v.y, v.z, v.w}}){};
    ClearValue(const glm::uvec4& v) : color(ClearColorValue{.uint32 = {v.x, v.y, v.z, v.w}}){};
    ClearValue& operator=(const glm::vec4& v)
    {
        color = {.float32 = {v.x, v.y, v.z, v.w}};
        return *this;
    }
    ClearValue(float depth, uint32_t stencil = 0) : depthStencil({depth, stencil}){};

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

    bool HasDepth() const
    {
        return depthStencilDescription.has_value();
    }
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
