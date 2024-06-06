#pragma once
#include "GfxDriver/GfxEnums.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Gfx
{
struct StencilOpState
{
    StencilOp failOp = StencilOp::Keep;
    StencilOp passOp = StencilOp::Keep;
    StencilOp depthFailOp = StencilOp::Keep;
    CompareOp compareOp = CompareOp::Never;
    uint32_t compareMask = 0xffffffff;
    uint32_t writeMask = 0xffffffff;
    uint32_t reference = 0;

    bool operator==(const StencilOpState& other) const noexcept = default;
};

struct ColorBlendAttachmentState
{
    bool blendEnable = false;
    BlendFactor srcColorBlendFactor = BlendFactor::Src_Alpha;
    BlendFactor dstColorBlendFactor = BlendFactor::One_Minus_Src_Alpha;
    BlendOp colorBlendOp = BlendOp::Add;
    BlendFactor srcAlphaBlendFactor = BlendFactor::Src_Alpha;
    BlendFactor dstAlphaBlendFactor = BlendFactor::One_Minus_Src_Alpha;
    BlendOp alphaBlendOp = BlendOp::Add;
    ColorComponentBits colorWriteMask = ColorComponentBit::Component_All_Bits;

    bool operator==(const ColorBlendAttachmentState& other) const noexcept = default;
};

struct ShaderConfig
{
    bool vertexInterleaved = false;
    bool debug = false;
    CullMode cullMode = CullMode::Back;
    Topology topology = Topology::TriangleList;
    PolygonMode polygonMode = PolygonMode::Fill;

    struct Depth
    {
        bool operator==(const Depth& other) const = default;
        bool writeEnable = true;
        bool testEnable = true;
        CompareOp compOp = CompareOp::Less_or_Equal;
        bool boundTestEnable = false;
        float minBounds = 0;
        float maxBounds = 1;
    } depth;

    struct Stencil
    {
        bool operator==(const Stencil& other) const = default;
        bool testEnable = false;
        StencilOpState front;
        StencilOpState back;
    } stencil;

    struct Color
    {
        bool operator==(const Color& other) const = default;
        std::vector<ColorBlendAttachmentState> blends;
        float blendConstants[4] = {1, 1, 1, 1};
    } color;

    std::vector<std::vector<std::string>> features;

    std::unordered_map<std::string, size_t> shaderInfoInputBaseTypeSizeOverride;

    bool operator==(const ShaderConfig& other) const noexcept = default;
};
} // namespace Gfx
