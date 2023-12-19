#include "ShaderProgram.hpp"

namespace Gfx
{
bool StencilOpState::operator==(const StencilOpState& other) const noexcept
{
    return failOp == other.failOp && passOp == other.passOp && depthFailOp == other.depthFailOp &&
           compareOp == other.compareOp && compareMask == other.compareMask && writeMask == other.writeMask &&
           reference == other.reference;
}

bool ColorBlendAttachmentState::operator==(const ColorBlendAttachmentState& other) const noexcept
{
    return blendEnable == other.blendEnable && srcColorBlendFactor == other.srcColorBlendFactor &&
           dstColorBlendFactor == other.dstColorBlendFactor && colorBlendOp == other.colorBlendOp &&
           srcAlphaBlendFactor == other.srcAlphaBlendFactor && dstAlphaBlendFactor == other.dstAlphaBlendFactor &&
           alphaBlendOp == other.alphaBlendOp && colorWriteMask == other.colorWriteMask;
}

bool ShaderConfig::operator==(const ShaderConfig& other) const noexcept
{
    return cullMode == other.cullMode && depth.writeEnable == other.depth.writeEnable &&
           depth.testEnable == other.depth.testEnable && depth.compOp == other.depth.compOp &&
           depth.boundTestEnable == other.depth.boundTestEnable && depth.minBounds == other.depth.minBounds &&
           depth.maxBounds == other.depth.maxBounds && stencil.testEnable == other.stencil.testEnable &&
           stencil.front == other.stencil.front && stencil.back == other.stencil.back &&
           color.blends == other.color.blends && color.blendConstants[0] == other.color.blendConstants[0] &&
           color.blendConstants[1] == other.color.blendConstants[1] &&
           color.blendConstants[2] == other.color.blendConstants[2] &&
           color.blendConstants[3] == other.color.blendConstants[3];
}
} // namespace Gfx
