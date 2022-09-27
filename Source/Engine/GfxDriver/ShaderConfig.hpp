#pragma once
#include "GfxDriver/GfxEnums.hpp"
#include <vector>

namespace Engine::Gfx
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

        bool operator==(const StencilOpState& other) const noexcept;
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

        bool operator==(const ColorBlendAttachmentState& other) const noexcept;
    };

    struct ShaderConfig
    {
        bool vertexInterleaved = true;
        CullMode cullMode = CullMode::Back;

        struct
        {
            bool writeEnable = false;
            bool testEnable = false;
            CompareOp compOp = CompareOp::Less_or_Equal;
            bool boundTestEnable = false;
            float minBounds = 0;
            float maxBounds = 1;
        } depth;

        struct
        {
            bool testEnable = false;
            StencilOpState front;
            StencilOpState back;
        } stencil;

        struct
        {
            std::vector<ColorBlendAttachmentState> blends;
            float blendConstants[4] = {1,1,1,1};
        } color;

        bool operator==(const ShaderConfig& other) const noexcept;
    };
}
