#pragma once
#include "Driver/GfxDriver/GfxEnums.hpp"
#include "Library/DynamicArray.hpp"
#include "Library/Hash.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

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

struct PipelineConfig
{
    struct PipelineConfig_t
    {
        struct Depth
        {
            bool operator==(const Depth& other) const = default;
            bool writeEnable = true;
            bool testEnable = true;
            CompareOp compOp = CompareOp::Greater_Or_Equal;
            bool boundTestEnable = false;
            float minBounds = 0;
            float maxBounds = 1;
            float depthBias = 0.0f;
            float depthSlopBias = 0.0f;
        };

        struct Stencil
        {
            bool operator==(const Stencil& other) const = default;
            bool testEnable = false;
            StencilOpState front;
            StencilOpState back;
        };

        struct Color
        {
            bool operator==(const Color& other) const = default;
            std::vector<ColorBlendAttachmentState> blends;
            float blendConstants[4] = {1, 1, 1, 1};
        };

        CullMode cullMode = CullMode::Back;
        Topology topology = Topology::TriangleList;
        PolygonMode polygonMode = PolygonMode::Fill;
        Depth depth = {};
        Stencil stencil = {};
        Color color = {};

        bool operator==(const PipelineConfig_t& other) const noexcept = default;
    };

    PipelineConfig() { Rehash(); };
    PipelineConfig(const PipelineConfig_t& other)
    {
        v = std::make_shared<PipelineConfig_t>(other);
        Rehash();
    }

    const PipelineConfig_t* operator->() const { return &*v; }

    PipelineConfig_t operator*() const { return *v; }

    PipelineConfig& operator=(const PipelineConfig_t& other)
    {
        v = std::make_shared<PipelineConfig_t>(other);
        Rehash();
        return *this;
    }

    PipelineConfig& operator=(const PipelineConfig& other)
    {
        v = other.v;
        hash = other.hash;

        return *this;
    }

    bool operator==(const PipelineConfig& other) const { return hash == other.hash; }

    uint64_t GetHash() const { return hash; }
    static PipelineConfig FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;

private:
    uint64_t hash = 0;
    std::shared_ptr<PipelineConfig_t> v = std::make_shared<PipelineConfig_t>();
    void Rehash();
};

class ShaderFeatures
{
    std::vector<std::vector<std::string>> features;
    std::vector<std::vector<std::string>> vertFeatures;
    std::vector<std::vector<std::string>> fragFeatures;
    std::unordered_map<std::string, size_t> shaderInfoInputBaseTypeSizeOverride;
};

} // namespace Gfx
