#pragma once
#include "GfxDriver/GfxEnums.hpp"
#include <nlohmann/json.hpp>
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
    std::vector<std::vector<std::string>> vertFeatures;
    std::vector<std::vector<std::string>> fragFeatures;

    std::unordered_map<std::string, size_t> shaderInfoInputBaseTypeSizeOverride;

    bool operator==(const ShaderConfig& other) const noexcept = default;

    static ShaderConfig FromJson(const nlohmann::json& j)
    {
        ShaderConfig config;

        // Basic settings
        config.vertexInterleaved = j.value("vertexInterleaved", false);
        config.debug = j.value("debug", false);
        config.cullMode = static_cast<CullMode>(j.value("cullMode", static_cast<int>(CullMode::Back)));
        config.topology = static_cast<Topology>(j.value("topology", static_cast<int>(Topology::TriangleList)));
        config.polygonMode = static_cast<PolygonMode>(j.value("polygonMode", static_cast<int>(PolygonMode::Fill)));

        // Depth settings
        if (j.contains("depth"))
        {
            const auto& depthJson = j["depth"];
            config.depth.writeEnable = depthJson.value("writeEnable", true);
            config.depth.testEnable = depthJson.value("testEnable", true);
            config.depth.compOp =
                static_cast<CompareOp>(depthJson.value("compOp", static_cast<int>(CompareOp::Less_or_Equal)));
            config.depth.boundTestEnable = depthJson.value("boundTestEnable", false);
            config.depth.minBounds = depthJson.value("minBounds", 0.0f);
            config.depth.maxBounds = depthJson.value("maxBounds", 1.0f);
        }

        // Stencil settings
        if (j.contains("stencil"))
        {
            const auto& stencilJson = j["stencil"];
            config.stencil.testEnable = stencilJson.value("testEnable", false);

            auto parseStencilOpState = [](const nlohmann::json& sJson) -> StencilOpState
            {
                StencilOpState state;
                state.failOp = static_cast<StencilOp>(sJson.value("failOp", static_cast<int>(StencilOp::Keep)));
                state.passOp = static_cast<StencilOp>(sJson.value("passOp", static_cast<int>(StencilOp::Keep)));
                state.depthFailOp =
                    static_cast<StencilOp>(sJson.value("depthFailOp", static_cast<int>(StencilOp::Keep)));
                state.compareOp = static_cast<CompareOp>(sJson.value("compareOp", static_cast<int>(CompareOp::Always)));
                state.compareMask = sJson.value("compareMask", 0);
                state.writeMask = sJson.value("writeMask", 0);
                state.reference = sJson.value("reference", 0);
                return state;
            };

            if (stencilJson.contains("front"))
            {
                config.stencil.front = parseStencilOpState(stencilJson["front"]);
            }
            if (stencilJson.contains("back"))
            {
                config.stencil.back = parseStencilOpState(stencilJson["back"]);
            }
        }

        // Color settings
        if (j.contains("color"))
        {
            const auto& colorJson = j["color"];
            if (colorJson.contains("blends"))
            {
                for (const auto& blendJson : colorJson["blends"])
                {
                    ColorBlendAttachmentState blend;
                    blend.blendEnable = blendJson.value("blendEnable", false);
                    blend.srcColorBlendFactor = static_cast<BlendFactor>(
                        blendJson.value("srcColorBlendFactor", static_cast<int>(BlendFactor::One))
                    );
                    blend.dstColorBlendFactor = static_cast<BlendFactor>(
                        blendJson.value("dstColorBlendFactor", static_cast<int>(BlendFactor::Zero))
                    );
                    blend.colorBlendOp =
                        static_cast<BlendOp>(blendJson.value("colorBlendOp", static_cast<int>(BlendOp::Add)));
                    blend.srcAlphaBlendFactor = static_cast<BlendFactor>(
                        blendJson.value("srcAlphaBlendFactor", static_cast<int>(BlendFactor::One))
                    );
                    blend.dstAlphaBlendFactor = static_cast<BlendFactor>(
                        blendJson.value("dstAlphaBlendFactor", static_cast<int>(BlendFactor::Zero))
                    );
                    blend.alphaBlendOp =
                        static_cast<BlendOp>(blendJson.value("alphaBlendOp", static_cast<int>(BlendOp::Add)));
                    blend.colorWriteMask = blendJson.value("colorWriteMask", 0);
                    config.color.blends.push_back(blend);
                }
            }

            if (colorJson.contains("blendConstants") && colorJson["blendConstants"].is_array())
            {
                nlohmann::json blendConstants = colorJson["blendConstants"];
                for (int i = 0; i < 4; ++i)
                {
                    config.color.blendConstants[i] = blendConstants[i];
                }
            }
        }

        // Features
        config.features = j.value("features", std::vector<std::vector<std::string>>());
        config.vertFeatures = j.value("vertFeatures", std::vector<std::vector<std::string>>());
        config.fragFeatures = j.value("fragFeatures", std::vector<std::vector<std::string>>());

        // Shader input base type size overrides
        if (j.contains("shaderInfoInputBaseTypeSizeOverride") && j["shaderInfoInputBaseTypeSizeOverride"].is_object())
        {
            for (const auto& [key, value] : j["shaderInfoInputBaseTypeSizeOverride"].items())
            {
                config.shaderInfoInputBaseTypeSizeOverride[key] = value.get<size_t>();
            }
        }

        return config;
    }

    nlohmann::json ToJson() const
    {
        nlohmann::json j;
        j["vertexInterleaved"] = vertexInterleaved;
        j["debug"] = debug;
        j["cullMode"] = static_cast<int>(cullMode);
        j["topology"] = static_cast<int>(topology);
        j["polygonMode"] = static_cast<int>(polygonMode);

        // Depth settings
        j["depth"] = nlohmann::json::object_t();
        j["depth"]["writeEnable"] = depth.writeEnable;
        j["depth"]["testEnable"] = depth.testEnable;
        j["depth"]["compOp"] = static_cast<int>(depth.compOp);
        j["depth"]["boundTestEnable"] = depth.boundTestEnable;
        j["depth"]["minBounds"] = depth.minBounds;
        j["depth"]["maxBounds"] = depth.maxBounds;

        // Stencil settings
        j["stencil"] = nlohmann::json::object_t();
        j["stencil"]["testEnable"] = stencil.testEnable;
        j["stencil"]["front"] = nlohmann::json::object_t();
        j["stencil"]["front"]["failOp"] = static_cast<int>(stencil.front.failOp);
        j["stencil"]["front"]["passOp"] = static_cast<int>(stencil.front.passOp);
        j["stencil"]["front"]["depthFailOp"] = static_cast<int>(stencil.front.depthFailOp);
        j["stencil"]["front"]["compareOp"] = static_cast<int>(stencil.front.compareOp);
        j["stencil"]["front"]["compareMask"] = stencil.front.compareMask;
        j["stencil"]["front"]["writeMask"] = stencil.front.writeMask;
        j["stencil"]["front"]["reference"] = stencil.front.reference;

        j["stencil"]["back"] = nlohmann::json::object_t();
        j["stencil"]["back"]["failOp"] = static_cast<int>(stencil.back.failOp);
        j["stencil"]["back"]["passOp"] = static_cast<int>(stencil.back.passOp);
        j["stencil"]["back"]["depthFailOp"] = static_cast<int>(stencil.back.depthFailOp);
        j["stencil"]["back"]["compareOp"] = static_cast<int>(stencil.back.compareOp);
        j["stencil"]["back"]["compareMask"] = stencil.back.compareMask;
        j["stencil"]["back"]["writeMask"] = stencil.back.writeMask;
        j["stencil"]["back"]["reference"] = stencil.back.reference;

        // Color settings
        j["color"] = nlohmann::json::object_t();
        j["color"]["blends"] = nlohmann::json::array();
        for (const auto& blend : color.blends)
        {
            nlohmann::json blendJson;
            blendJson["blendEnable"] = blend.blendEnable;
            blendJson["srcColorBlendFactor"] = static_cast<int>(blend.srcColorBlendFactor);
            blendJson["dstColorBlendFactor"] = static_cast<int>(blend.dstColorBlendFactor);
            blendJson["colorBlendOp"] = static_cast<int>(blend.colorBlendOp);
            blendJson["srcAlphaBlendFactor"] = static_cast<int>(blend.srcAlphaBlendFactor);
            blendJson["dstAlphaBlendFactor"] = static_cast<int>(blend.dstAlphaBlendFactor);
            blendJson["alphaBlendOp"] = static_cast<int>(blend.alphaBlendOp);
            blendJson["colorWriteMask"] = blend.colorWriteMask;
            j["color"]["blends"].push_back(blendJson);
        }
        j["color"]["blendConstants"] =
            {color.blendConstants[0], color.blendConstants[1], color.blendConstants[2], color.blendConstants[3]};

        // Features
        j["features"] = features;
        j["vertFeatures"] = vertFeatures;
        j["fragFeatures"] = fragFeatures;

        // Shader input base type size overrides
        j["shaderInfoInputBaseTypeSizeOverride"] = nlohmann::json::object();
        for (const auto& [key, value] : shaderInfoInputBaseTypeSizeOverride)
        {
            j["shaderInfoInputBaseTypeSizeOverride"][key] = value;
        }

        return j;
    }
};
} // namespace Gfx
